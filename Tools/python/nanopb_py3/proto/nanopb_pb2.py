'''nanopb's options (nanopb.proto) as Python 3 classes.

Built at import time from the descriptor set the build writes with protoc
(Usagi/_build/proto/nanopb.desc, or $USAGI_NANOPB_DESC), so it always matches
the nanopb.proto in the tree, including Usagi's custom options, and works
with any protobuf runtime from 3.20 on.
'''
import os

from google.protobuf import descriptor_pb2 as _descriptor_pb2
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf.internal import builder as _builder

_usagi_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '..', '..')
_desc_path = os.environ.get('USAGI_NANOPB_DESC') or \
    os.path.join(_usagi_dir, '_build', 'proto', 'nanopb.desc')

with open(_desc_path, 'rb') as _f:
    _set = _descriptor_pb2.FileDescriptorSet.FromString(_f.read())
_file = [_fd for _fd in _set.file if os.path.basename(_fd.name) == 'nanopb.proto'][0]

DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(_file.SerializeToString())
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, globals())
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'nanopb_pb2', globals())

# The pure-Python runtime (protobuf < 4) only parses and serialises an
# extension once it is registered with the message class it extends; newer
# runtimes find extensions through the pool and don't need (or have) this.
for _ext in DESCRIPTOR.extensions_by_name.values():
    _extendee = getattr(_descriptor_pb2, _ext.containing_type.name, None)
    if _extendee is not None and hasattr(_extendee, 'RegisterExtension'):
        _extendee.RegisterExtension(_ext)
