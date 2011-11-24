////////////////////////////////////////////////////////////////////////////////
//    mainApp.cpp - main OpenFrameworks application
//    Copyright (C) 2011  Rob Myers <rob@robmyers.org>
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "drawing.h"
#include "eeg.h"
#include "expression.h"
#include "layout_options.h"
#include "twitterStreaming.h"

#include "mainApp.h"


mainApp::mainApp(ofxArgs* args){
  this->args = args;
}


//--------------------------------------------------------------
void mainApp::setup(){
  po::variables_map vm;
  layout_initialize(vm);

  if(this->args->contains("-data")){
    this->data_path = this->args->getString("-data");
  }else{
    std::cerr << "Please specify -data /path/to-data/directory" << std::endl;
    ::exit(-1);
  }

  load_emotions(this->data_path);

  drawing_setup();

  std::string twitter_userpass;
  if(this->args->contains("-twitter")){
    twitter_userpass = this->args->getString("-twitter");
  }else{
    std::cerr << "Please specify -twitter username:password" << std::endl;
    ::exit(-1);
  }

  ofSetWindowTitle("upload");
  ofSetFrameRate(60);

  start_twitter_search(twitter_userpass);
}

//--------------------------------------------------------------
void mainApp::update(){
  static int count = 0;
  count++;
  if (count == 100){
    reset_twitter_emotion_map();
    count = 0;
  }
  update_state();
  update_display_data();
}

//--------------------------------------------------------------
void mainApp::draw(){
  ofSetColor(0xFFFFFFFF);
  ofDrawBitmapString(current_emotion.c_str(), 100, 100);
  draw_eegs();
  draw_face();
}


//--------------------------------------------------------------
void mainApp::keyPressed(int key){
}

//--------------------------------------------------------------
void mainApp::keyReleased(int key){
}

//--------------------------------------------------------------
void mainApp::mouseMoved(int x, int y ){
}

//--------------------------------------------------------------
void mainApp::mouseDragged(int x, int y, int button){
}

//--------------------------------------------------------------
void mainApp::mousePressed(int x, int y, int button){
}

//--------------------------------------------------------------
void mainApp::mouseReleased(int x, int y, int button){
}

//--------------------------------------------------------------
void mainApp::windowResized(int w, int h){
}
