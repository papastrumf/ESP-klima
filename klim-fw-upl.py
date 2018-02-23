#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
Created on Wed Aug 24 15:13:42 2016

@author: ja
"""

import sys
import datetime
import subprocess
import getopt
import urllib2


def main():
    dt1 = datetime.datetime.now()
    dt2 = dt1.strftime("%y%m%d%H%M%S")
    ime = "klim_" + ( "%08x" % int(FW_VER, 16) ) + "_" + dt2 + ".bin"
    #dst = "app-root/repo/app/assets/depot/" + ime
    dst = "kek0.net:rails/oblak/app/assets/depot/" + ime
    #print "rhc scp ruby1 upload %s app-root/repo/app/assets/depot/%s" % (FILENM, ime)
    cmdpfx = ["scp", "-i", "/home/ja/.ssh/do_rsa", FILENM, dst ]
    
    cmdli = list(cmdpfx)
    #cmdli.append("")
    proc1 = subprocess.Popen(cmdli, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc1.communicate()
    print out
    if(len(err) > 0):
        print("*1* %s" % err)
        #quit()

    lnk = "http://oblak.kek0.net/vatra/fwupl?ver=" + FW_VER + "&dat=" + ime
    req = urllib2.Request(lnk)
    try:
        resp = urllib2.urlopen(req)
    except urllib2.HTTPError as e:
        print("%s %s" % (e.code, e.read()))
    else:
        odg = resp.read().rstrip()
        print odg
            


if __name__ == '__main__':
    try:
        opts, args = getopt.getopt(sys.argv[1:],"v:f:",["FW_VER=","FILENM="])
    except getopt.GetoptError:
        print 'klim-fw-upl.py -v <version> -f <filename>'
        sys.exit(2)
    for opt, arg in opts:
        #print "opt: %s; arg: %s" % (opt, arg) 
        if opt == '-h' or len(sys.argv) != 5:
            print 'klim-fw-upl.py -v <version> -f <filename>'
            sys.exit()
        elif opt in ("-v", "--version"):
            FW_VER = arg
        elif opt in ("-f", "--filename"):
            FILENM = arg
        if opt == '-h' or len(sys.argv) != 5:
            print 'klim-fw-upl.py -v <version> -f <filename>'
            sys.exit()
            
    try:
        FW_VER and FILENM
    except:
        print 'klim-fw-upl.py -v <version> -f <filename>'
        sys.exit()
        
        
    #print("opcije: %s %s" % ( FW_VER, FILENM ))
    main()
