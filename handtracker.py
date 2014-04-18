#!/bin/python

from threading import Thread
import subprocess
import numpy

class HandTracker(object):

    def __init__(self, dots):

        self.dots = dots
        self.run = True
        # exec handtracking/dsHandTracker.py
        self.proc = subprocess.Popen(
                ["python2", "handtracking/dsHandTracker.py"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
        )

        # save pipe on self
        t = Thread(target=self.track)
        t.daemon = True
        t.start()

    def track(self):
        while self.run:
            data = self.proc.stdout.readline()
            if b'RESET\n' == data:
                self.dots.reset()
                continue
            
            points = numpy.fromstring(data, dtype="int32", sep=",")
            if len(points) == 3:
                self.dots.add(points[0], points[1], points[2])


    def shutdown(self):
        self.run = False
        return

if __name__ == "__main__":
    ht = HandTracker(None);
    while True:
        pass
