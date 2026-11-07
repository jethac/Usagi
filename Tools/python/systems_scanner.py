#!/usr/bin/env python3
# Usagi Engine, Copyright (c) Vitei, Inc. 2013
"""systems_scanner.py: find Usagi systems in C++ sources and describe them as YAML.

A portable replacement for Tools/bin/systems-scanner.exe, built on libclang's
Python bindings. It is run by Tools/ruby/boilerplate_scan.rb with the same
arguments as the exe:

    systems_scanner.py [-I<dir>]... [-D<def>]... [-d<depfile>] -o <out.yml> <input>

<input> is a header that #includes the system sources. Every system visible in
the translation unit is written to <out.yml> in the format the exe used (one
flow mapping per system, sorted by priority then name, CRLF line endings), and
the files the input depends on are written to <depfile> in gcc format.

A system is
  - a class or struct definition deriving (directly or not) from usg::System,
    which isn't a template; or
  - a typedef of a specialisation of a class template system
    ("typedef CacheComponent<TransformComponent> CacheTransform;"), reported
    with isTemplated: true and read from the template's definition.

For each system:
  priority        the value of its CATEGORY constant (DECLARE_SYSTEM(...))
  inputs, outputs, colliderInputs, raycastHitInputs, triggererInputs
                  the Required<>/Optional<> fields of the nested structs Inputs,
                  Outputs, ColliderInputs, RaycastHitInputs and TriggererInputs:
                  status 1 = Required, 2 = Optional; ownerBitfield is the
                  search mask's ::value (FromSelf 1, FromParents 2,
                  FromSelfOrParents 3) or 0 when it has none (FromParentWith<>)
  signals         "<Method>Signal" for each static signal handler it declares,
                  in declaration order; OnEvent overloads become
                  "OnEventSignal< ::<event type> >"
  autogen*        its AUTOGENERATE_* switches (set by DECLARE_SYSTEM,
                  COLLIDABLE, RAYCASTER, TRIGGERABLE and EXCLUSION)
  isClass         declared with 'class' rather than 'struct'

Diagnostics from the parse are not errors: system sources include their own
generated .bp.cpp, which doesn't exist yet when they are scanned. Set
USAGI_SCANNER_VERBOSE=1 to see them. USAGI_LIBCLANG selects the libclang
shared library (default: the system's, then the one bundled with the
'libclang' pip package).
"""

import ctypes
import os
import sys

import clang.cindex as ci
from clang.cindex import CursorKind, TypeKind

SYSTEM_BASE = 'usg::System'

# Static member functions that respond to a signal. The signal is the
# function's name with 'Signal' appended.
SIGNAL_HANDLERS = {
    'Run', 'LateUpdate', 'GPUUpdate', 'OnCollision', 'OnTrigger', 'OnRaycastHit',
}
EVENT_HANDLER = 'OnEvent'

COLLECTIONS = [
    ('inputs', 'Inputs'),
    ('outputs', 'Outputs'),
    ('colliderInputs', 'ColliderInputs'),
    ('raycastHitInputs', 'RaycastHitInputs'),
    ('triggererInputs', 'TriggererInputs'),
]

AUTOGEN_FLAGS = [
    ('autogenGetInputOutputs', 'AUTOGENERATE_GET_INPUT_OUTPUTS'),
    ('autogenGetColliderInputs', 'AUTOGENERATE_GET_COLLIDER_INPUTS'),
    ('autogenGetRaycastHitInputs', 'AUTOGENERATE_GET_RAYCASTHIT_INPUTS'),
    ('autogenGetTriggererInputs', 'AUTOGENERATE_GET_TRIGGERER_INPUTS'),
    ('autogenCheckExclusions', 'AUTOGENERATE_CHECK_EXCLUSIONS'),
]

RECORD_KINDS = (CursorKind.CLASS_DECL, CursorKind.STRUCT_DECL)
SCOPE_KINDS = (CursorKind.NAMESPACE, CursorKind.LINKAGE_SPEC, CursorKind.UNEXPOSED_DECL)
TYPEDEF_KINDS = (CursorKind.TYPEDEF_DECL, CursorKind.TYPE_ALIAS_DECL)

VERBOSE = os.environ.get('USAGI_SCANNER_VERBOSE') == '1'


class ScanError(Exception):
    pass


# ---------------------------------------------------------------------------
# libclang setup
# ---------------------------------------------------------------------------

def load_libclang():
    candidates = []
    if os.environ.get('USAGI_LIBCLANG'):
        candidates.append(os.environ['USAGI_LIBCLANG'])
    candidates += ['/usr/lib/libclang.so', '/usr/lib64/libclang.so',
                   '/usr/lib/x86_64-linux-gnu/libclang.so',
                   '/usr/local/opt/llvm/lib/libclang.dylib']
    resource_dir = os.environ.get('USAGI_CLANG_RESOURCE_DIR')
    for lib in candidates:
        if os.path.exists(lib):
            ci.Config.set_library_file(lib)
            if resource_dir is None:
                resource_dir = find_resource_dir(os.path.dirname(os.path.realpath(lib)))
            break
    # Otherwise cindex finds the library bundled with the pip package.

    lib = ci.conf.lib
    lib.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
    lib.clang_Cursor_Evaluate.restype = ctypes.c_void_p
    lib.clang_EvalResult_getKind.argtypes = [ctypes.c_void_p]
    lib.clang_EvalResult_getKind.restype = ctypes.c_int
    lib.clang_EvalResult_getAsLongLong.argtypes = [ctypes.c_void_p]
    lib.clang_EvalResult_getAsLongLong.restype = ctypes.c_longlong
    lib.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
    lib.clang_EvalResult_dispose.restype = None
    return lib, resource_dir


def find_resource_dir(libdir):
    """clang's builtin headers (stddef.h, ...) for a libclang in libdir.

    libclang loaded through ctypes has no driver path to work this out from,
    so look for <libdir>/clang/<version>/include ourselves."""
    root = os.path.join(libdir, 'clang')
    if not os.path.isdir(root):
        return None
    found = []
    for v in os.listdir(root):
        d = os.path.join(root, v)
        if os.path.exists(os.path.join(d, 'include', 'stddef.h')):
            key = [int(x) if x.isdigit() else 0 for x in v.split('.')]
            found.append((key, d))
    return max(found)[1] if found else None


CXEval_Int = 1


def evaluate_int(cursor):
    """Integer value of a constant variable's initialiser, or None."""
    lib = ci.conf.lib
    res = lib.clang_Cursor_Evaluate(cursor)
    if not res:
        return None
    try:
        if lib.clang_EvalResult_getKind(res) != CXEval_Int:
            return None
        return int(lib.clang_EvalResult_getAsLongLong(res))
    finally:
        lib.clang_EvalResult_dispose(res)


def evaluate_var(cursor):
    """Evaluate a static const member, falling back to an enumerator it names."""
    value = evaluate_int(cursor)
    if value is not None:
        return value
    for c in cursor.walk_preorder():
        if c.kind == CursorKind.DECL_REF_EXPR and c.referenced is not None:
            ref = c.referenced
            if ref.kind == CursorKind.ENUM_CONSTANT_DECL:
                return ref.enum_value
            if ref.kind == CursorKind.VAR_DECL and ref != cursor:
                v = evaluate_var(ref)
                if v is not None:
                    return v
        elif c.kind == CursorKind.INTEGER_LITERAL:
            toks = [t.spelling for t in c.get_tokens()]
            if toks:
                return int(toks[0].rstrip('uUlL'), 0)
    return None


# ---------------------------------------------------------------------------
# Names and types
# ---------------------------------------------------------------------------

def qualified_name(cursor):
    parts = []
    c = cursor
    while c is not None and c.kind != CursorKind.TRANSLATION_UNIT:
        if c.spelling:
            parts.append(c.spelling)
        c = c.semantic_parent
    return '::'.join(reversed(parts))


def definition_of(cursor):
    if cursor is None:
        return None
    d = cursor.get_definition()
    return d if d is not None else cursor


def type_name(t):
    """Fully qualified canonical spelling of a type, without a leading '::'."""
    s = t.get_canonical().spelling
    for prefix in ('struct ', 'class ', 'enum ', 'union '):
        if s.startswith(prefix):
            s = s[len(prefix):]
    return s


def template_args(t):
    """Template argument types of a (possibly sugared or dependent) specialisation."""
    for candidate in (t, t.get_canonical()):
        n = candidate.get_num_template_arguments()
        if n > 0:
            return [candidate.get_template_argument_type(i) for i in range(n)]
    return []


def template_name(t):
    """Unqualified name of the template a type specialises ('Required')."""
    decl = t.get_canonical().get_declaration()
    if decl is not None and decl.kind != CursorKind.NO_DECL_FOUND:
        tmpl = ci.conf.lib.clang_getSpecializedCursorTemplate(decl)
        if tmpl is not None and tmpl.kind != CursorKind.INVALID_FILE:
            if tmpl.spelling:
                return tmpl.spelling
        if decl.spelling:
            return decl.spelling
    # Dependent specialisation: take the name from the spelling
    s = t.spelling
    return s.split('<', 1)[0].split('::')[-1].strip()


# ---------------------------------------------------------------------------
# Class hierarchy
# ---------------------------------------------------------------------------

def bases(record):
    for c in record.get_children():
        if c.kind == CursorKind.CXX_BASE_SPECIFIER:
            decl = c.type.get_canonical().get_declaration()
            if decl is None or decl.kind == CursorKind.NO_DECL_FOUND:
                ref = c.referenced
                decl = ref
            if decl is not None and decl.kind != CursorKind.NO_DECL_FOUND:
                yield definition_of(decl)


def derives_from_system(record, seen=None):
    if seen is None:
        seen = set()
    for b in bases(record):
        if b is None:
            continue
        name = qualified_name(b)
        if name == SYSTEM_BASE:
            return True
        key = b.hash
        if key in seen:
            continue
        seen.add(key)
        if derives_from_system(b, seen):
            return True
    return False


def find_member(record, name, kinds, seen=None):
    """A member declared in the record or one of its bases other than usg::System."""
    for c in record.get_children():
        if c.spelling == name and c.kind in kinds:
            return c
    if seen is None:
        seen = set()
    for b in bases(record):
        if b is None or qualified_name(b) == SYSTEM_BASE or b.hash in seen:
            continue
        seen.add(b.hash)
        found = find_member(b, name, kinds, seen)
        if found is not None:
            return found
    return None


# ---------------------------------------------------------------------------
# System description
# ---------------------------------------------------------------------------

class System(object):
    def __init__(self, name, source_file, record, is_templated, is_class):
        self.name = name
        self.source_file = source_file
        self.record = record          # class definition (template pattern if templated)
        self.is_templated = is_templated
        self.is_class = is_class

    def member_type_name(self, t):
        """Spell a component type; types declared inside a templated system are
        named through the system's typedef ('...::CacheTransform::CacheIntervalComp')."""
        if self.is_templated:
            decl = t.get_declaration()
            if decl is not None and decl.kind in TYPEDEF_KINDS:
                parent = decl.semantic_parent
                if parent is not None and parent.kind in (CursorKind.CLASS_TEMPLATE,) + RECORD_KINDS \
                        and parent == self.record:
                    return '%s::%s' % (self.name.lstrip(':'), decl.spelling)
            if t.kind == TypeKind.UNEXPOSED or 'type-parameter' in t.get_canonical().spelling:
                s = t.spelling
                if '::' not in s:
                    return '%s::%s' % (self.name.lstrip(':'), s)
        return type_name(t)

    def collection(self, struct_name):
        struct = find_member(self.record, struct_name, RECORD_KINDS)
        if struct is None:
            return []
        struct = definition_of(struct)
        entries = []
        for field in struct.get_children():
            if field.kind != CursorKind.FIELD_DECL:
                continue
            wrapper = template_name(field.type)
            if wrapper == 'Required':
                status = 1
            elif wrapper == 'Optional':
                status = 2
            else:
                continue
            args = template_args(field.type)
            if not args:
                raise ScanError('cannot read the component type of %s::%s::%s'
                                % (self.name, struct_name, field.spelling))
            owner = 1
            if len(args) > 1:
                owner = search_mask_value(args[1])
            entries.append((field.spelling, self.member_type_name(args[0]), status, owner))
        return entries

    def priority(self):
        var = find_member(self.record, 'CATEGORY', (CursorKind.VAR_DECL,))
        if var is None:
            return 0
        value = evaluate_var(var)
        if value is None:
            raise ScanError('cannot evaluate %s::CATEGORY' % self.name)
        return value

    def switch(self, name):
        var = find_member(self.record, name, (CursorKind.VAR_DECL,))
        if var is None:
            return False
        value = evaluate_var(var)
        return bool(value)

    def signals(self):
        out = []
        for c in self.record.get_children():
            if c.kind != CursorKind.CXX_METHOD or not c.is_static_method():
                continue
            name = c.spelling
            if name in SIGNAL_HANDLERS:
                out.append('%sSignal' % name)
            elif name == EVENT_HANDLER:
                params = [a for a in c.get_arguments()]
                if len(params) < 3:
                    continue
                evt = params[2].type
                if evt.kind in (TypeKind.LVALUEREFERENCE, TypeKind.RVALUEREFERENCE, TypeKind.POINTER):
                    evt = evt.get_pointee()
                evt = evt.get_canonical()
                # Strip const and other qualifiers: spell the canonical type
                spelled = type_name(evt)
                if spelled.startswith('const '):
                    spelled = spelled[len('const '):]
                out.append('OnEventSignal< ::%s >' % spelled)
        return out

    def to_yaml(self):
        def coll(entries):
            return '[' + ', '.join(
                '{name: "%s", type: "%s", status: %d, ownerBitfield: %d}' % e
                for e in entries) + ']'

        def b(v):
            return 'true' if v else 'false'

        parts = ['name: "%s"' % self.name,
                 'sourceFile: "%s"' % self.source_file,
                 'priority: %d' % self.priority()]
        for key, struct in COLLECTIONS:
            parts.append('%s: %s' % (key, coll(self.collection(struct))))
        parts.append('signals: [' + ', '.join('"%s"' % s for s in self.signals()) + ']')
        for key, var in AUTOGEN_FLAGS:
            parts.append('%s: %s' % (key, b(self.switch(var))))
        parts.append('isTemplated: %s' % b(self.is_templated))
        parts.append('isClass: %s' % b(self.is_class))
        return '{ ' + ', '.join(parts) + ' }'


def search_mask_value(t):
    decl = t.get_canonical().get_declaration()
    if decl is None or decl.kind == CursorKind.NO_DECL_FOUND:
        return 0
    decl = definition_of(decl)
    for c in decl.get_children():
        if c.kind == CursorKind.VAR_DECL and c.spelling == 'value':
            v = evaluate_var(c)
            if v is None:
                # The constant didn't parse (a header scanned without
                # Engine/Common/Common.h has no uint32); use the known masks
                v = KNOWN_SEARCH_MASKS.get(qualified_name(decl), 0)
            return v
    return 0


# Engine/Framework/Component.h
KNOWN_SEARCH_MASKS = {'usg::FromSelf': 1, 'usg::FromParents': 2, 'usg::FromSelfOrParents': 3}


def is_class_keyword(cursor):
    for tok in cursor.get_tokens():
        if tok.spelling in ('class', 'struct'):
            return tok.spelling == 'class'
        break
    return cursor.kind == CursorKind.CLASS_DECL


def file_name(cursor):
    f = cursor.location.file
    return f.name if f is not None else ''


def collect_systems(tu, input_path):
    systems = {}

    def visit(parent):
        for c in parent.get_children():
            k = c.kind
            if k in SCOPE_KINDS:
                visit(c)
            elif k in RECORD_KINDS:
                if not c.is_definition():
                    continue
                name = qualified_name(c)
                if name == SYSTEM_BASE or not name:
                    continue
                if derives_from_system(c):
                    full = '::' + name
                    if full not in systems:
                        systems[full] = System(full, file_name(c), c, False,
                                               c.kind == CursorKind.CLASS_DECL)
            elif k in TYPEDEF_KINDS:
                under = c.underlying_typedef_type.get_canonical()
                decl = under.get_declaration()
                if decl is None or decl.kind == CursorKind.NO_DECL_FOUND:
                    continue
                if under.get_num_template_arguments() <= 0:
                    continue
                tmpl = ci.conf.lib.clang_getSpecializedCursorTemplate(decl)
                if tmpl is None or tmpl.kind != CursorKind.CLASS_TEMPLATE:
                    continue
                tmpl = definition_of(tmpl)
                if not derives_from_system(tmpl):
                    continue
                full = '::' + qualified_name(c)
                if full not in systems:
                    systems[full] = System(full, file_name(c), tmpl, True, False)

    visit(tu.cursor)
    return [s for s in systems.values() if os.path.abspath(s.source_file) != input_path]


def write_depfile(path, out, tu, input_path):
    deps = []
    seen = set()
    for inc in tu.get_includes():
        name = inc.include.name
        if name in seen or os.path.abspath(name) == input_path:
            continue
        seen.add(name)
        deps.append(name)
    with open(path, 'w', newline='\n') as f:
        f.write(out.replace(' ', '\\ ') + ':')
        for d in deps:
            f.write(' \\\n  ' + d.replace(' ', '\\ '))
        f.write('\n')


def parse_args(argv):
    clang_args = []
    out = None
    depfile = None
    inputs = []
    newline = '\r\n'
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '-o':
            i += 1
            out = argv[i]
        elif a.startswith('-o') and len(a) > 2:
            out = a[2:]
        elif a.startswith('-d') and len(a) > 2:
            depfile = a[2:]
        elif a == '-d':
            i += 1
            depfile = argv[i]
        elif a == '--lf':
            newline = '\n'
        elif a in ('-I', '-D', '-U', '-include'):
            clang_args += [a, argv[i + 1]]
            i += 1
        elif a.startswith(('-I', '-D', '-U', '-std', '-W', '-f', '-isystem')):
            clang_args.append(a)
        elif a == '-H':
            pass
        elif a.startswith('-'):
            clang_args.append(a)
        else:
            inputs.append(a)
        i += 1
    if out is None or len(inputs) != 1:
        raise ScanError('usage: systems_scanner.py [-I<dir>] [-D<def>] [-d<depfile>] -o <out.yml> <input>')
    return clang_args, out, depfile, inputs[0], newline


def main(argv):
    clang_args, out, depfile, input_file, newline = parse_args(argv)
    _, resource_dir = load_libclang()

    args = ['-x', 'c++', '-std=c++17', '-fsyntax-only', '-w',
            '-Wno-everything', '-ferror-limit=0']
    if resource_dir:
        args += ['-resource-dir', resource_dir]
    args += clang_args
    index = ci.Index.create()
    try:
        tu = index.parse(input_file, args=args,
                         options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES |
                                 ci.TranslationUnit.PARSE_INCOMPLETE)
    except ci.TranslationUnitLoadError as e:
        raise ScanError('libclang could not parse %s: %s' % (input_file, e))

    if VERBOSE:
        for d in tu.diagnostics:
            sys.stdout.write('%s\n' % d)

    input_path = os.path.abspath(input_file)
    systems = collect_systems(tu, input_path)
    rows = sorted(((s.priority(), s.name, s) for s in systems), key=lambda r: (r[0], r[1]))

    lines = ['Systems:']
    for _, _, s in rows:
        lines.append('  - ' + s.to_yaml())
    text = newline.join(lines) + newline

    d = os.path.dirname(out)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(out, 'w', newline='') as f:
        f.write(text)

    if depfile:
        write_depfile(depfile, out, tu, input_path)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except ScanError as e:
        sys.stderr.write('systems_scanner: %s\n' % e)
        sys.exit(1)
