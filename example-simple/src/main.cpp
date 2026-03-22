#include "ofMain.h"
#include "ofApp.h"

// You can set these variables to customise the compiler
// Be sure to set them before any "ofxCppSketch.cpp"
#define OFXCPPSKETCH_CUSTOM_COMPILER "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
#define OFXCPPSKETCH_CUSTOM_STD_VERSION " -std=c++17"
#define OFXCPPSKETCH_CUSTOM_MAC_SDK "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk"
#define OFXCPPSKETCH_CUSTOM_EXTRA_FLAGS " -O0 -target x86_64-apple-macosx10.15-macho -pipe -fexceptions -fvisibility=default -Wno-unused-parameter -Werror=return-type -fPIC -DDEBUG -DGCC_HAS_REGEX"

#include "ofxCppSketch.h"

//========================================================================
int main( ){

	ofSetupOpenGL(1024,768, OF_WINDOW);			// <-------- setup the GL context

	// this kicks off the running of my app
	// can be OF_WINDOW or OF_FULLSCREEN
	// pass in width and height too:
	ofRunApp(new ofxCppSketch("ofApp", __FILE__));
}
