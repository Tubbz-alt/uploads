#!/usr/bin/env python

# capture_expressions.py - Capture someone's emoting face using a Kinect
# Copyright (C) 2011, 2012  Rob Myers rob@robmyers.org
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or 
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.


################################################################################
# Imports
################################################################################

import cv
import glob
import os
import numpy
from PIL import Image
import sys
import time

import freenect

import calibkinect

import emotions


################################################################################
# Configuration
################################################################################

# How often per second to try to capture a Kinect frame
KINECT_CAPTURE_FREQUENCY = 10
KINECT_CAPTURE_TIMING = 1.0 / KINECT_CAPTURE_FREQUENCY

KINECT_FRAME_WIDTH = 640
KINECT_FRAME_HEIGHT = 480

KINECT_FRAME_FORMAT = 'png'


################################################################################
# Kinect
################################################################################

class KinectFrame(object):
    
    def getFrame(self):
        self.when = time.time()
        depth,_ = freenect.sync_get_depth()
        rgb,_ = freenect.sync_get_video()
        # Retain a copy otherwise we crash later
        self.depth = numpy.copy(depth)
        self.rgb = numpy.copy(rgb)

    def xyzuv(self):
        q = self.depth
        X,Y = numpy.meshgrid(range(KINECT_FRAME_WIDTH),
                             range(KINECT_FRAME_HEIGHT))
        return calibkinect.depth2xyzuv(q,X,Y)
    
    
    def dumpFrame(self, path):
        """Save rgb, xyz and uv data for frame as tsv files with simple names"""
        # Save as png.
        # png is 10x the size of jpeg, but we only halve the data per second
        # if we use jpeg as the other data is already compressed
        mode = 'RGBA'
        size = (KINECT_FRAME_WIDTH, KINECT_FRAME_HEIGHT)
        # Swizzle the rgb array to a form PIL will like
        rgb = self.rgb.reshape(self.rgb.shape[0] * self.rgb.shape[1],
                                 self.rgb.shape[2])
        rgb = numpy.c_[rgb, 255 * numpy.ones((len(rgb), 1), numpy.uint8)]
        # Inefficiently convert the array to a PIL image
        pil_image = Image.frombuffer(mode, size, rgb.tostring(), 'raw', mode,
                                     0, 1)
        pil_image.save(os.path.join(path, 
                                    "%s.%s" % (self.when, KINECT_FRAME_FORMAT)),
                       optimize=True)
        xyz, uv = self.xyzuv()
        # Save as gzipped tsv. Bulky text rather than binary incompatibility
        uvpath = os.path.join(path, "%s.uv.gz" % self.when)
        numpy.savetxt(uvpath, uv, delimeter='\t')
        xyzpath = os.path.join(path, "%s.xyz.gz" % self.when)
        numpy.savetxt(xyzpath, xyz, delimeter='\t')
    
    def displayFrame(self):
        # Just show the rgb data for speed
        # NO, need depth image to ensure we aren't too close to the camera
        cv.ShowImage('camera', self.rgb)#[::2,::2,::-1])
        cv.ShowImage('depth(ish)', self.depth)
        cv.WaitKey(5)


def handleKinect(frames, last_frame_time, now):
    """If it's time to grab a new frame, grab and save it"""
    new_time = last_frame_time
    if now - last_frame_time > KINECT_CAPTURE_TIMING:
        frame = KinectFrame()
        frame.getFrame()
        frames.append(frame)
        new_time = now
        frame.displayFrame()
    return new_time


def dumpFrames(frames, folder):
    for frame in frames:
        frame.dumpFrame(folder)


################################################################################
# Emotion capturing
################################################################################


def capture_emotion(captured_emotions, person_name, emotion, duration):
    """Capture the emotions ready to save"""
    while True:
        print "Please start [pretending that you are] feeling %s" % emotion
        print "I am going to start capturing data in %s seconds" % \
            emotions.SECONDS_TO_WAIT_BEFORE_CAPTURING
        time.sleep(emotions.SECONDS_TO_WAIT_BEFORE_CAPTURING)
        frames = []
        last_frame = time.time()
        finish = last_frame + emotions.SECONDS_TO_CAPTURE_EMOTION_FOR
        while last_frame < finish:
            last_frame = handleKinect(frames, last_frame, time.time())
        print "Done. Did you manage to hold the feeling the entire time? [y/n]"
        if raw_input().lower().strip() in ["y", "yes", ""]:
            captured_emotions[emotion] = frames
            break
        else:
            print "Trying again..."

def save_emotions(captured_emotions, person_name):
    """Save the collected emotions"""
    print "Saving to file:"
    for emotion in captured_emotions:
        print "\t" + emotion
        person_emotion_path = os.path.join(person_name, emotion)
        # The eeg capture may already have made this folder
        try:
            os.mkdir(person_emotion_path)
        except:
            pass
        dumpFrames(captured_emotions[emotion], person_emotion_path)
    print "Done."

def capture_emotions(person_name, emotions, duration):
    """Capture each emotion in turn"""
    captured_emotions = {}
    print """I am going to prompt you to pretend to feel the following emotions for %s seconds each: %s""" % (duration, ', '.join(emotions))
    for emotion in emotions:
        capture_emotion(captured_emotions, person_name, emotion, duration)
    freenect.sync_stop()
    save_emotions(captured_emotions, person_name)


################################################################################
# Main flow of execution
################################################################################


def usage():
    """Print usage instructions"""
    print "USAGE: %s person_name" % sys.argv[0]
    sys.exit(1)


def new_emotions(person_name, emotion_names):
    """List the emotions that need capturing"""
    emotions = []
    for emotion in emotion_names:
        # Folder may already exist due to capture_emotion, so check for files
        if not glob.glob(os.path.join(person_name, emotion,
                                      "*.%s" % KINECT_FRAME_FORMAT)):
            emotions.append(emotion)
    if emotions == []:
        print "All emotions captured for %s, exiting." % person_name
        sys.exit(0)
    return emotions


def main():
    """Capture emotions."""
    if len(sys.argv) != 2:
        usage()
    person_name = sys.argv[1]
    emotion_names = emotions.EMOTIONS
    if os.path.exists(person_name):
        print "Folder for %s exists. Adding any missing faces." % person_name
        emotion_names = new_emotions(person_name, emotion_names) 
    else:
        os.mkdir(person_name)
    capture_emotions(person_name, emotion_names,
                     emotions.SECONDS_TO_CAPTURE_EMOTION_FOR)


if __name__ == "__main__":
    main()
