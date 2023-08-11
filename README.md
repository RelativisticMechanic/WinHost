# WinHost

A utility written in C++ using Windows sockets that acts as a simple HTTP server

## Description

When you run this program from a directory, it will begin serving web pages on your localhost. 

## Usage

Simply compile using build-winhost.bat, you may need to edit the path of your vcvars64.bat which sets the environment variables for MSVC.

The default port is 8888, if you wish to change this, ehm... I tried to add port as a dynamic argument but Windows defender flags it as a Trojan. So, yeah, if you want a unique port just change the port value in main() to something else. 