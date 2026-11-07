#!/usr/bin/env python3
'''Runs a nanopb-style protoc plugin under Python 3.

    run_plugin.py <generator.py>

protoc 2.5's Python output (nanopb's checked-in proto/nanopb_pb2.py and
plugin_pb2.py, and what the build regenerates in their place) is Python 2
only. This puts the 'proto' package next to this script first on sys.path,
so the generator imports Python 3 equivalents of those two modules, and then
runs the generator unchanged as a protoc plugin.

nanopb's generator (a vitei fork, in a submodule) is otherwise Python 3
ready except for one call: it passes a str to zlib.crc32() for the message
IDs. Rather than fork the submodule, crc32 here also accepts str, encoded as
UTF-8, which gives the value Python 2 computed for the same (ASCII) name.
Its old-style regex escapes ('\\*') only raise a SyntaxWarning; those are
silenced so protoc's output stays clean.
'''
import os
import runpy
import sys
import warnings
import zlib

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)

_crc32 = zlib.crc32
def _crc32_str(data, value=0):
    if isinstance(data, str):
        data = data.encode('utf-8')
    return _crc32(data, value)
zlib.crc32 = _crc32_str

warnings.filterwarnings('ignore', category=SyntaxWarning)

generator = os.path.abspath(sys.argv[1])
sys.argv = [generator, '--protoc-plugin'] + sys.argv[2:]
runpy.run_path(generator, run_name='__main__')
