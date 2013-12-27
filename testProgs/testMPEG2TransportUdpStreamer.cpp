/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// Copyright (c) 1996-2013, Live Networks, Inc.  All rights reserved
// A test program that receives a UDP multicast stream
// and retransmits it to another (multicast or unicast) address & port
// main program

#include "liveMedia.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"

// To stream using "source-specific multicast" (SSM), uncomment the following:
//#define USE_SSM 1

#define TRANSPORT_PACKET_SIZE 188
#define TRANSPORT_PACKETS_PER_NETWORK_PACKET 7
// The product of these two numbers must be enough to fit within a network packet

UsageEnvironment* env;
char const* inputFileName = "test.ts";
FramedSource* videoSource;
MediaSink* videoSink;

void play(); // forward

int main(int argc, char** argv) {
  // Begin by setting up our usage environment:
  TaskScheduler* scheduler = BasicTaskScheduler::createNew();
  env = BasicUsageEnvironment::createNew(*scheduler);

  if (argc == 1) {
    *env << "Usage: " << argv[0] << " [-f file] [-I iface address] [-m mcast address] [-p udp port]\n";
    exit(1);
  }

  // Create 'groupsocks' for UDP:
  char const* destinationAddressStr
#ifdef USE_SSM
    = "232.255.42.42";
#else
  = "239.255.42.42";
  // Note: This is a multicast address.  If you wish to stream using
  // unicast instead, then replace this string with the unicast address
  // of the (single) destination.  (You may also need to make a similar
  // change to the receiver program.)
#endif
  char const* sendingInterfaceAddrStr = "0.0.0.0";

  unsigned short udpPortNum = 1234;
  const unsigned char ttl = 7; // low, in case routers don't admin scope

  int ch;
  while ((ch = getopt(argc, argv, "f:I:m:p:")) != EOF) {
    switch(ch) {
      case 'f':
        inputFileName = optarg;
        break;
      case 'I': {
        sendingInterfaceAddrStr = optarg;
        NetAddressList addresses(sendingInterfaceAddrStr);
        if (addresses.numAddresses() == 0) {
          *env << "Failed to find network address for \"" << optarg << "\"\n";
          exit(1);
        }
        SendingInterfaceAddr = *(unsigned*)(addresses.firstAddress()->data());
        break;
      }
      case 'm':
        destinationAddressStr = optarg;
        break;
      case 'p':
        udpPortNum = atoi(optarg);
        break;
      default:
        break;
    }
  }


  struct in_addr destinationAddress;
  destinationAddress.s_addr = our_inet_addr(destinationAddressStr);
  const Port udpPort(udpPortNum);

  Groupsock udpGroupsock(*env, destinationAddress, udpPort, ttl);
#ifdef USE_SSM
  udpGroupsock.multicastSendOnly();
#endif

  // Create an appropriate 'UDP sink' from the UDP 'groupsock':
  unsigned const maxPacketSize = 1316; // allow for large UDP packets
  videoSink = BasicUDPSink::createNew(*env, &udpGroupsock, maxPacketSize);


  // Finally, start the streaming:
  *env << "Beginning streaming...\n";
  *env << "Multicast address: " << destinationAddressStr<< "\n";
  *env << "Interface address: " << sendingInterfaceAddrStr << "\n";
  *env << "UDP port: " << udpPortNum << "\n";
  *env << "File: " << inputFileName << "\n";

  play();

  env->taskScheduler().doEventLoop(); // does not return

  return 0; // only to prevent compiler warning
}

void afterPlaying(void* /*clientData*/) {
  *env << "...done reading from file\n";

  videoSink->stopPlaying();
  Medium::close(videoSource);
  // Note that this also closes the input file that this source read from.

  play();
}

void play() {
  unsigned const inputDataChunkSize
    = TRANSPORT_PACKETS_PER_NETWORK_PACKET*TRANSPORT_PACKET_SIZE;

  // Open the input file as a 'byte-stream file source':
  ByteStreamFileSource* fileSource
    = ByteStreamFileSource::createNew(*env, inputFileName, inputDataChunkSize);
  if (fileSource == NULL) {
    *env << "Unable to open file \"" << inputFileName
	 << "\" as a byte-stream file source\n";
    exit(1);
  }

  // Create a 'framer' for the input source (to give us proper inter-packet gaps):
  videoSource = MPEG2TransportStreamFramer::createNew(*env, fileSource);

  // Finally, start playing:
  *env << "Beginning to read from file...\n";
  videoSink->startPlaying(*videoSource, afterPlaying, videoSink);
}

