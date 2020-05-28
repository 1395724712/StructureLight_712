#!/usr/bin/python

import cv2
from numpy import *
import sys
from PIL import Image
width  = 964 
height = 1292

nstripes = 30
alpha = 2*pi/3 # phase shift 120 degree

pattern_dir = "./pattern"

def genFringe(h,w):

    fringe1 = zeros((h,w))
    fringe2 = zeros((h,w))
    fringe3 = zeros((h,w))

    # periodlength:
    phi = w/nstripes
    
    # scale factor delta = 2pi/phi
    delta = 2*pi/phi

    f = lambda x,a : (sin(x*delta+a) +1)* 120
    
    # compute a row of fringe pattern
    sinrow1 = [f(x,-alpha) for x in xrange(w)]
    sinrow2 = [f(x,0) for x in xrange(w)]
    sinrow3 = [f(x,alpha) for x in xrange(w)]
    
    fringe1[:,:] = sinrow1
    fringe2[:,:] = sinrow2
    fringe3[:,:] = sinrow3

    return fringe1,fringe2,fringe3

def genFringeVert():
    imgarr = genFringe(width,height)
    a = array(imgarr,uint8)
    index = 1
    for i in a:
        im = Image.fromarray(i)        
        im.save(pattern_dir + "/Fringe_vert" + str(index) + ".bmp")
        index = index + 1    

def genFringeHor():
    imgarr = genFringe(width,height)
    a = array(imgarr,uint8)
    index = 1
    for i in a:
        im = Image.fromarray(i.transpose())        
        im.save(pattern_dir + "/Fringe_hor" + str(index) + ".bmp")
        index = index + 1

if __name__ == "__main__":

    nstripes = 108 #条纹数量

    # genFringeHor() # 水平条纹
    genFringeVert() # 竖直条纹


