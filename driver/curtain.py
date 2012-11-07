import argparse
import ConfigParser
import datetime
import logging
import os
import sys
import subprocess
import time

class XGrabber(object):
    def __init__(self, executable = 'xgrabber', font = None, no_lock = False):
        self.args = [ executable ]
        if no_lock:
            self.args.append('-n')
        if font is not None:
            self.args += [ '-f', font ]
        self.proc = None
        self.text = ''
        self.ensure_started()
    
    def ensure_started(self):
        if self.proc:
            self.proc.poll()
            if self.proc.returncode is not None:
                logging.error('xgrabber terminated')
                self.proc = None
        if self.proc is None:
            self.proc = subprocess.Popen(self.args, stdin=subprocess.PIPE)
        self.proc.stdin.write(self.text + '\n')

    def display_text(self, text):
        self.text = text
        self.ensure_started()

    def unlock(self):
        self.proc.kill()

class FakeSecHead(object):
    def __init__(self, fp):
        self.fp = fp
        self.sechead = '[DEFAULT]\n'
    def readline(self):
        if self.sechead:
            try: return self.sechead
            finally: self.sechead = None
        else: return self.fp.readline()

class DriverConfigParser(object):
    def __init__(self, filename = None):
        SECTION = 'DEFAULT'
        config = ConfigParser.RawConfigParser(defaults={
            'enabled': 'true',
            'message': 'Screen locked',
            })
        if filename:
            config.readfp(FakeSecHead(open(filename)))
        self.enabled = config.getboolean(SECTION, 'enabled')
        if not self.enabled:
            return
        self.message_template = config.get(SECTION, 'message')
        if config.has_option(SECTION, 'font'):
            self.font = config.get(SECTION, 'font')
        else:
            self.font = None
        if config.has_option(SECTION, 'until'):
            self.until = datetime.datetime.strptime(config.get(SECTION, 'until'), '%Y/%m/%d %H:%M:%S')
        else:
            self.until = None

    def should_lock(self):
        if not self.enabled:
            return False
        if self.until is None:
            return True
        return datetime.datetime.now() < self.until

    def message(self):
        dict = { }
        dict['date'] = datetime.datetime.now().strftime('%Y/%m/%d')
        dict['time'] = datetime.datetime.now().strftime('%H:%M:%S')
        if self.until:
            # a hack to format time difference in HH:MM:SS format
            remaining = self.until - datetime.datetime.now()
            dict['remaining'] = (datetime.datetime(2000,1,1) + remaining).strftime('%H:%M:%S')
        return self.message_template.format(**dict)

def test_config(config_file):
    config = DriverConfigParser(config_file)
    config.should_lock()
    if config.enabled:
        config.message()
        
def mainloop(config_name, dry_run):
    config = DriverConfigParser()
    grabber = None
    
    while True:
        try:
            new_config = DriverConfigParser(args.config)
            config = new_config
        except Exception as e:
            logging.error('Cannot open config %s: %s', args.config, e)
        if config.should_lock():
            if grabber is None:
                logging.info('Starting xgrabber')
                grabber = XGrabber(font = config.font, no_lock = args.dry_run)
                logging.info('Xgrabber started')
            grabber.display_text(config.message())
        else:
            if grabber:
                logging.info('Killing xgrabber')
                grabber.unlock()
                grabber = None
                logging.info('Xgrabber killed')
        time.sleep(0.2)

argparser = argparse.ArgumentParser()
argparser.add_argument('-t', '--test', action='store_true', help='Load config and verify its correctness')
argparser.add_argument('-n', '--dry-run', action='store_true', help='Do not grab keyboard and mouse')
argparser.add_argument('config', help='Path to config file')
args = argparser.parse_args()

if args.test:
    test_config(args.config)
else:
    mainloop(args.config, args.dry_run)

# vim: set ts=4 sts=4 sw=4 tw=0 et:
