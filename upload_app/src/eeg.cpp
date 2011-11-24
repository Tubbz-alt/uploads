////////////////////////////////////////////////////////////////////////////////
//    eeg.cpp - load serialized eeg data
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


////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>

#include "emotion.h"

#include "eeg.h"


////////////////////////////////////////////////////////////////////////////////
// Timestamps
// EEG samples are taken at given times, expressed as timestamps
////////////////////////////////////////////////////////////////////////////////

// Normalize timestamps in eeg sample objects in a vector to be relative to
// the first as time 0

template<class T>
void normalize_timestamps(T & values, double base)
{
  for(typename T::iterator i = values.begin(); i < values.end(); ++i)
  {
    (*i).timestamp = (*i).timestamp - base;
  }
}


////////////////////////////////////////////////////////////////////////////////
// EEG Power Levels
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> values_names = 
  {"poor signal level", "low alpha", "high alpha", "low beta", "high beta",
   "low gamma", "high gamma", "attention", "meditation"};

// Slurp power_levels from a tsv file into a vector

void slurp_power_levels(const std::string & file,
			power_levels_vector & levels_vector)
{
  std::ifstream source(file);
  if(!source.is_open())
    throw std::runtime_error("Couldn't open file " + file);
  while(source)
  {
    std::string line;
    std::getline(source, line);
    // Skip comments
    if(line.length() > 0 && line[0] == '#')
      continue;
    // We get an empty line at the end???
    if(line.length() == 0)
      break;
    std::vector<std::string> fields;
    boost::split(fields, line, boost::is_any_of("\t"));
    power_levels levels;
    levels.timestamp = ::atof(fields[0].c_str());
    levels.values = {::atoi(fields[1].c_str()), 
		     ::atoi(fields[2].c_str()), ::atoi(fields[4].c_str()),
		     ::atoi(fields[4].c_str()), ::atoi(fields[5].c_str()),
		     ::atoi(fields[6].c_str()), ::atoi(fields[7].c_str()),
		     ::atoi(fields[8].c_str()), ::atoi(fields[9].c_str())};
    assert(levels.timestamp != 0.0);
    levels_vector.push_back(levels);	      
  }
}


////////////////////////////////////////////////////////////////////////////////
// Raw EEG data
////////////////////////////////////////////////////////////////////////////////

// Slurp raw eeg data from a tsv file into a vector

void slurp_raw_eeg(const std::string & file, raw_eeg_vector & eeg_vector)
{
  std::ifstream source(file);
  if(!source.is_open())
    throw std::runtime_error("Couldn't open file " + file);
  while(source)
  {
    std::string line;
    std::getline(source, line);
    // Skip comments
    if(line.length() > 0 && line[0] == '#')
      continue;
    // We get an empty line at the end???
    if(line.length() == 0)
      break;
    std::vector<std::string> fields;
    boost::split(fields, line, boost::is_any_of("\t"));
    raw_eeg eeg = {
      ::atof(fields[0].c_str()), ::atoi(fields[1].c_str())
    };
    assert(eeg.timestamp != 0.0);
    eeg_vector.push_back(eeg);				      
  }
}

// Truncate raw eeg samples (e) to same timestamp range as power levels (L)
// so if we start with LeeeeeeeLeeeeeLeeeeeLee
//  we should get: LeeeeeeeLeeeeeLeeeeeL

void truncate_eeg_to_levels_timestamps(raw_eeg_vector & eeg, 
				       power_levels_vector & levels)
{
  double levels_begin = levels.front().timestamp;
  double levels_end = levels.back().timestamp;
  // incredibly inefficient. find_if & rbegin then truncate isn't happy on gcc4
  std::remove_if(eeg.begin(), eeg.end(), 
		 [&levels_begin](raw_eeg & e)
		 {return e.timestamp < levels_begin;});
  std::find_if(eeg.begin(), eeg.end(), 
	       [&levels_end](raw_eeg & e)
	       {return e.timestamp > levels_end;});
}


////////////////////////////////////////////////////////////////////////////////
// The map of emotions to eeg data
////////////////////////////////////////////////////////////////////////////////

// The map of emotion names to eeg data

emotion_data_map emotion_datas;

// The names of the files in each emotion directory

const std::string RAW_EEG_FILENAME("raw_eeg.txt");
const std::string POWER_LEVELS_FILENAME("power_levels.txt");

// Make the path to the relevant file for the relevant emotion for the user

std::string emotion_file(const std::string & user_name,
			 const std::string & emotion,
			 const std::string & data_file)
{
  return user_name + "/" + emotion + "/" + data_file;
}

// Load the raw eeg data file for the emotion for the user

void load_emotion_raw_eeg_data(const std::string & user_name,
			       const std::string & emotion,
			       raw_eeg_vector & eeg_vector)
{
  slurp_raw_eeg(emotion_file(user_name, emotion, RAW_EEG_FILENAME),
		eeg_vector);
}

// Load the power levels data file for the emotion for the user

void load_emotion_power_levels_data(const std::string & user_name,
				    const std::string & emotion,
				    power_levels_vector & levels_vector)
{
  slurp_power_levels(emotion_file(user_name, emotion, POWER_LEVELS_FILENAME),
		     levels_vector);
}

// Load all the user's emotion data files from each emotion folder

void load_emotions(const std::string & username)
{
  BOOST_FOREACH(const std::string & emotion, emotions)
  {
    emotion_data data;
    data.name = emotion;
    load_emotion_power_levels_data(username, emotion, data.power_levels);
    load_emotion_raw_eeg_data(username, emotion, data.raw_eeg);
    // Ensure power levels and raw eeg data have the same range
    truncate_eeg_to_levels_timestamps(data.raw_eeg, data.power_levels);
    // Then make the timestamps on each relative to the first timestamp
    double relative_to = data.power_levels[0].timestamp;
    normalize_timestamps(data.power_levels, relative_to);
    normalize_timestamps(data.raw_eeg, relative_to);
    data.max_levels_time = data.power_levels.back().timestamp;
    emotion_datas[emotion] = data;
  }
}


////////////////////////////////////////////////////////////////////////////////
// The current EEG data for drawing
////////////////////////////////////////////////////////////////////////////////

// The number of items of data to plot

static const unsigned int levels_to_display = 30;
static const unsigned int eeg_to_display = 200;

// The current time, mostly for debugging

double last_update_at = 0.0;

// The current emotion, from Twitter

emotion_data current_emotion_data;

// The current positions in the current emotion's data

raw_eeg_iterator current_eeg_iterator;
power_levels_iterator current_levels_iterator;

// The data to plot visually

std::deque<power_levels> levels_display_data;
std::deque<raw_eeg> eeg_display_data;

// Set a data item iterator to the current time
// Used in initialization and when the current emotion is changed

template<class T>
typename T::iterator set_current_iterator(float current_time_mod, T & source)
{
  typename T::iterator iter = source.begin();
  while((*iter).timestamp < current_time_mod)
  {
    iter++;
  }
  return iter;
}

// Take data from the source vector and push into the destination vector
// This wraps round carefully

template<class T, class U>
void update_data_vector(double now_mod,
			double previous_mod,
			T & source,
			U & destination,
			typename T::iterator & iter)
{
  typename T::iterator end = source.end();
  // if we've wrapped round
  // in pathalogical cases of system delay we will miss multiple passes
  //  if that happens there are probably worse things to worry about
  //  and the code won't fail, it
  if(now_mod < previous_mod)
  {
    // push the remaining values
    for(; iter != end; ++iter)
    {
      destination.push_front(*iter);
    }
    // and wrap round
    iter = source.begin();
  }
  // whether we looped or not, keep pushing values until we hit the current time
  while((*iter).timestamp <= now_mod)
  {
    // push the new value
    destination.push_front(*iter);
    // Move on to the next data item
    ++iter;
  }
  // Note that the iterator is now pointing to the next item,
  //  which means it will be at a time *after* the current time
}


////////////////////////////////////////////////////////////////////////////////
// Update the drawing data as time elapses
////////////////////////////////////////////////////////////////////////////////

// Get the current time in the range 0..max_timestamp

double current_time(emotion_data & data)
{
  timeval tv;
  ::gettimeofday(&tv, NULL);
  return std::fmod(tv.tv_sec + (tv.tv_usec/1000000.0), data.max_levels_time);
}

// set the current emotion name and data
//  and create the initial iterators
// this must be done after we get the first update from Twitter

void update_state()
{
  current_emotion_data = emotion_datas[current_emotion];
  double now = current_time(current_emotion_data);
  last_update_at = now;
  //std::cerr << "update_state " << current_emotion << " " << now << std::endl;
  current_eeg_iterator =
    set_current_iterator(now, current_emotion_data.raw_eeg);
  current_levels_iterator =
    set_current_iterator(now, current_emotion_data.power_levels);
}

// update the display data
// the strategy is to push data until the iterator catches up to now,
//  then truncate the display data if too long
// note that our use of globals makes this non-threadsafe

double update_display_data()
{
  double now = current_time(current_emotion_data);
  //std::cerr << "updating at " << now << std::endl;
  // Push new data
  //update_data_vector(now, last_update_at,
  //		     current_emotion_data.raw_eeg, eeg_display_data,
  //		     current_eeg_iterator);
  update_data_vector(now, last_update_at,
		     current_emotion_data.power_levels,
		     levels_display_data, current_levels_iterator);
  // Truncate old data
  eeg_display_data.resize(std::min(eeg_display_data.size(), eeg_to_display));
  levels_display_data.resize(std::min(levels_display_data.size(),
				      levels_to_display));
  last_update_at = now;
  return now;
}


////////////////////////////////////////////////////////////////////////////////
// Testing, debugging, etc.
////////////////////////////////////////////////////////////////////////////////

// print a *bit* of information about the emotion

void print_emotion(const emotion_data & emo)
{
  std::cerr << emo.name << std::endl;
  //const power_levels_vector & levels = emo.power_levels;
  std::cerr << "eeg start/end:" << std::endl;
  const raw_eeg estart = emo.raw_eeg.front(); 
  const raw_eeg eend = emo.raw_eeg.back(); 
  std::cerr << std::fixed << estart.timestamp << " " 
	      << estart.value << std::endl;
  std::cerr << std::fixed << eend.timestamp << " " 
	      << eend.value << std::endl;
  std::cerr << "levels start/end:" << std::endl;
  power_levels lstart = emo.power_levels.front(); 
  power_levels lend = emo.power_levels.back(); 
  std::cerr << std::fixed << lstart.timestamp << " " 
	    << lstart.values[power_levels::low_gamma] << std::endl;
  std::cerr << std::fixed << lend.timestamp << " " 
	    << lend.values[power_levels::low_gamma] << std::endl;
}

// print the current global state

void print_current_state()
{
  std::cerr << "Now: " << last_update_at << std::endl;
  std::cerr << current_emotion 
	    << ": "
	    << current_emotion_data.name
	    << std::endl;
  std::cerr << "eeg iterator timestamp: " << (*current_eeg_iterator).timestamp
	    << std::endl;
  std::cerr << "level iterator timestamp: " 
	    << (*current_levels_iterator).timestamp
	    << std::endl;
  std::cerr << "levels data (len " << levels_display_data.size() << ")" 
	    << std::endl;
  for(int i = 0; i < std::min(5, (int)levels_display_data.size()); i++)
    std::cerr << levels_display_data[i].timestamp << ": "
	      << levels_display_data[i].values[power_levels::low_gamma] << " ";
  if(! levels_display_data.empty())
    std::cerr << std::endl;
  std::cerr << "eeg data (len " << eeg_display_data.size() << ")" 
	    << std::endl;
  for(int i = 0; i < std::min(5, (int)eeg_display_data.size()); i++)
    std::cerr << eeg_display_data[i].timestamp << ": "
	      << eeg_display_data[i].value << " ";
  if(! eeg_display_data.empty())
    std::cerr << std::endl;
}

void data_test()
{
  emotion_data & emo = emotion_datas["love"];
  print_emotion(emo);
  current_emotion = "love";
  update_state();
  print_current_state();
  for(int i = 0; i < 50; i++)
  {
    ::usleep(250000);
    update_display_data();
    print_current_state();
  }
}
