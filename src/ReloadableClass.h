//
//  MZGL
//
//  Created by Marek Bereza on 15/01/2018.
//  Copyright © 2018 Marek Bereza. All rights reserved.
//
/*
 
 
 for this to work, you need to put this in your Other C++ flags
 -DSRCROOT=\"${SRCROOT}\"
 
 
 found some good nuggets here
 https://glandium.org/blog/?p=2764
 about how to do precompiled headers.
 
 What this does is precompile headers when the app starts if we're doing a LiveCodeApp.
 It shouldn't really be in here, this should be for a general RecompilingDylib
 
 */

#pragma once
#include "FileWatcher.h"
#include "Dylib.h"
#include "liveCodeUtils.h"
#include <sys/stat.h>
#include <fstream>
#include <vector>

using namespace std;

template <class T>
class ReloadableClass {
public:

	ReloadableClass(){
		setDefaults();
	}

	
	Dylib dylib;
	
	FileWatcher watcher;
	string path;
	
	function<void()> reloadStarted;
	function<void(T*)> reloaded;
	function<void()> willCloseDylib;
	
	function<void(const string&)> reloadFailed;
	vector<string> includePaths;
	string prefixHeader;
	string cppFile = "";
	
	/**
	 * Parameters:
	 *  path - path to the header file of the live class - must have the same name as the class itself
	 *  includePaths - optional all the paths to include
	 *  headerToPrecompile - optional - if you pass in a main header file, it will be precompiled for faster loading times.
	 */
	void init(const string &path, const vector<string> &includePaths = {}, const string &headerToPrecompile = "") {
		
		liveCodeUtils::init();
		this->includePaths = includePaths;
		this->path = findFile(path);
		
		prefixHeader = headerToPrecompile;
		if(headerToPrecompile!="") {
			precompileHeader(headerToPrecompile);
		}
		
		watcher.watch(this->path);
		
		cppFile = getCppFileForHeader(this->path);
		
		if(cppFile!="") {
			watcher.watch(this->cppFile);
		}
		
		watcher.touched = [this]() {
			recompile();
		};
	}
	
	// if you want to recompile this class when other files change
	// (they might be dependents for this class) you can add them
	// here manually.
	void addFileToWatch(string path) {
		watcher.watch(path);
	}
	
	void checkForChanges() {
		watcher.tick();
	}
	
	string getCppFileForHeader(string p) {
		int lastDot = p.rfind('.');
		if(lastDot==-1) return "";
		string cpp = p.substr(0, lastDot) + ".cpp";
		
		if(File(cpp).exists()) {
			printf("cpp file is at %s\n", cpp.c_str());
			return cpp;
		}
		return "";
	}
	
	void precompileHeader(const string &headerToPrecompile) {
		std::string cmd = getCompilerBaseCmd();
		cmd += " -x c++-header -stdlib=libc++ "
			+ liveCodeUtils::includeListToCFlags(includePaths)
			+ " " + headerToPrecompile;
		printf("Precompiling headers: %s\n", cmd.c_str());
		liveCodeUtils::execute(cmd);
	}
#ifdef APPLE
	void setMacosSdk(string sdkPath){
		osxSDK = sdkPath;
	}
	string getMacosSdk() const {
		return osxSDK;
	}
#endif
	void setCompiler(string compilerStr){
		compiler = compilerStr;
	}
	string getCompiler() const {
		return compiler;
	}
	void setExtraCompilerFlags(string extraFlags){
		extraCompilerFlags = " ";
		extraCompilerFlags += extraFlags;
	}
	string getExtraCompilerFlags() const {
		return extraCompilerFlags;
	}
	void setStdVersion(int version){
		if(version < 11){
			printf("Invalid STD version : %i ! (minimum=11)", version);
			return;
		}
		stdVersion = " -std=c++";
		stdVersion += std::to_string(version);
	}
	void setStdVersion(std::string version){
		if(version.find("c++")!=0u){
			printf("Invalid STD version : %s (it must start with `c++`)", version.c_str());
			return;
		}
		stdVersion = " -std=";
		stdVersion += version;
	}
	string getStdVersionStr() const {
		return stdVersion;
	}
	
	
private:
	string compiler = "";
	string stdVersion = "";
	string extraCompilerFlags = "";
#ifdef __APPLE__
	string osxSDK = "";
#endif
	string lastErrorStr;
	void recompile() {
		//printf("Need to recompile %s\n", path.c_str());
		float t = liveCodeUtils::getSeconds();

		string dylibPath = cc();
		if(dylibPath!="") {
			if(reloadStarted) reloadStarted();
			auto *obj = loadDylib(dylibPath);
			if(obj != nullptr) {
				printf("\xE2\x9C\x85\xE2\x9C\x85\xE2\x9C\x85 Success loading \xE2\x9C\x85\xE2\x9C\x85\xE2\x9C\x85\n");
				printf("Compile took %.0fms\n", (liveCodeUtils::getSeconds() - t)*1000.f);
				if(reloaded) reloaded(obj);
			} else {
				printf("\xE2\x9D\x8C\xE2\x9D\x8C\xE2\x9D\x8C Error: No dice loading dylib \xE2\x9D\x8C\xE2\x9D\x8C\xE2\x9D\x8C\n");
				if(reloadFailed) reloadFailed("Couldn't read dylib\n");
			}

		} else {
			if(reloadFailed) reloadFailed(lastErrorStr);
		}
	}
	
	string getAllIncludes() {
		string userIncludes = liveCodeUtils::includeListToCFlags(includePaths);
		return " -I. "//getAllIncludes(string(SRCROOT))
		+ userIncludes
		// this is the precomp header + " -include " + string(LIBROOT) + "/mzgl/App.h"
		//+ getAllIncludes(string(LIBROOT)) + "/mzgl/ "
		//+ getAllIncludes(string(LIBROOT)+"/../opt/dsp/")
		;
	}
	
	string cc() {
		// call our makefile
		string objectName = getObjectName(path);
		string objFile = "/tmp/" + objectName + ".o";
		string cppFile = "/tmp/"+objectName + ".cpp";
		string dylibPath = "/tmp/" + objectName + ".dylib";
		makeCppFile(cppFile, objectName);

		string includes = getAllIncludes();
		
		string prefixHeaderFlag = "";
		if(prefixHeader!="") {
			prefixHeaderFlag = " -include " + prefixHeader + " ";
		}
		
		std::string cmd = getCompilerBaseCmd();
		cmd += " -g -stdlib=libc++" + prefixHeaderFlag + " -Wno-deprecated-declarations -c " + cppFile + " -o " + objFile + " "
					+ includes;
		
		int exitCode = 0;
		string res =  liveCodeUtils::execute(cmd, &exitCode);
		
		//printf("Exit code : %d\n", exitCode);
		if(exitCode==0) {
			cmd = getCompilerBaseCmd() + " -dynamiclib -g -undefined dynamic_lookup -o "+dylibPath + " " + objFile;
			liveCodeUtils::execute(cmd);
			return dylibPath;
		} else {
			lastErrorStr = res;
			return "";
		}
	}
	
	
	
	
	void makeCppFile(string path, string objName) {
		ofstream outFile(path.c_str());
		
		outFile << "#include \""+objName+".h\"\n\n";
		outFile << "extern \"C\" {\n\n";
		outFile << "\n\nvoid *getPluginPtr() {return new "+objName+"(); };\n\n";
		outFile << "}\n\n";

		if(cppFile!="") {
			// include the contents of the cpp file if it exists
			ifstream inFile(cppFile);
			string line;
			if (inFile.is_open()) {
			  while ( getline (inFile,line) ) {
				outFile << line << '\n';
			  }
			  inFile.close();
			} else {
				printf("Error reading %s\n", cppFile.c_str());
			}
		}
		

		outFile.close();
	}
	
	string getObjectName(string p) {
		// split on last '/'
		int indexOfLastSlash = p.rfind('/');
		if(indexOfLastSlash!=-1) {
			p = p.substr(indexOfLastSlash + 1);
		}
		
		// split on last '.'
		int indexOfLastDot = p.rfind('.');
		if(indexOfLastDot!=-1) {
			p = p.substr(0, indexOfLastDot);
		}
		return p;
	}
	
	T *loadDylib(string dylibPath) {

		if(dylib.isOpen()) {
			if(willCloseDylib) willCloseDylib();
			dylib.close();
		}
		if(!dylib.open(dylibPath)) {
			return nullptr;
		}
		return (T*) dylib.get("getPluginPtr");
	}

	
	/////////////////////////////////////////////////////////
	
	
	string findFile(string file) {
		string f = file;
		if(File(f).exists()) return f;
		f = "src/" + f;
		if(File(f).exists()) return f;
		f = "../" + f;
		if(File(f).exists()) return f;
		//f = string(SRCROOT) + "/" + file;
		//if(fileExists(f)) return f;
		f = Directory::cwd() + "/" + file;
		if(File(f).exists()) return f;
		printf("Error: can't find source file %s\n", file.c_str());
		return f;
	}

	std::string getCompilerBaseCmd(){
		std::string cmd = compiler;

		// Add c++ std version
		cmd += stdVersion;

#ifdef __APPLE__
		if(!osxSDK.empty()){
			cmd += " -isysroot ";
			cmd += osxSDK;
		}
#endif
		cmd += extraCompilerFlags;

		return cmd;
	}

	void setDefaults(){
		// STD VERSION
		stdVersion = " -std=c++11"; // default
		// Auto-detect from build settings
#ifdef __cplusplus
	#if __cplusplus == 202002L
		stdVersion = " -std=c++20";
	#elif __cplusplus == 201703L
		stdVersion = " -std=c++17";
	#elif __cplusplus == 201402L
		stdVersion = " -std=c++14";
	#endif
#endif
		// Use custom one ?
#ifdef OFXCPPSKETCH_CUSTOM_STD_VERSION
		stdVersion = OFXCPPSKETCH_CUSTOM_STD_VERSION;
#endif

		// COMPILER
		compiler = OFXCPPSKETCH_DEFAULT_COMPILER;
#ifdef __APPLE__
		// Use Xcode compiler
		std::string tmpCompiler = liveCodeUtils::macGetCompilerPath();
		if(!tmpCompiler.empty()){
			compiler = tmpCompiler;
		}
#endif
		// Use custom one ?
#ifdef OFXCPPSKETCH_CUSTOM_COMPILER
		compiler = OFXCPPSKETCH_CUSTOM_COMPILER;
#endif

		// APPLE SDK
#ifdef __APPLE__
		// Grab compiler from xcode settings
		std::string sysRootSDK = liveCodeUtils::macGetSysRoot();
		if(!sysRootSDK.empty()){
			osxSDK = sysRootSDK;
		}
#endif
		// Use custom one ?
#ifdef OFXCPPSKETCH_CUSTOM_MAC_SDK
		osxSDK = OFXCPPSKETCH_CUSTOM_MAC_SDK;
#endif

		// EXTRA SETTINGS
#ifdef OFXCPPSKETCH_CUSTOM_EXTRA_FLAGS
		extraCompilerFlags = OFXCPPSKETCH_CUSTOM_EXTRA_FLAGS;
#else
		extraCompilerFlags = "";
#endif
	}
};
