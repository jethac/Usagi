import sys, os, os.path, subprocess
from optparse import Option, OptionParser

if sys.version_info[0] != 2:
    sys.stderr.write("ERROR: lvl2manifest.py requires Python 2.7; run through tools/usagi-dev-env.ps1 or put B:/usagi_dev/tools/python-2.7.14-x64 first on PATH.\n")
    sys.exit(1)

import LevelEditor
import Level2Yaml
import Level2Instances
import yaml

class MultipleOption(Option):
    ACTIONS = Option.ACTIONS + ("extend",)
    STORE_ACTIONS = Option.STORE_ACTIONS + ("extend",)
    TYPED_ACTIONS = Option.TYPED_ACTIONS + ("extend",)
    ALWAYS_TYPED_ACTIONS = Option.ALWAYS_TYPED_ACTIONS + ("extend",)

    def take_action(self, action, dest, opt, value, values, parser):
        if action == "extend":
            values.ensure_value(dest, []).append(value)
        else:
            Option.take_action(self, action, dest, opt, value, values, parser)

# Option parser
optparser = OptionParser(option_class=MultipleOption)
optparser.add_option("--dist", dest="distance", type="float")

(options, args) = optparser.parse_args()

def fail( message ):
    sys.stderr.write( message )
    return 1

def convert_manifest( args ):
    if len( args ) != 2:
        return fail( 'ERROR: Expected input level and manifest output path.\n' )

    # Early out if the specified level file doesn't exist.
    levelPath = args[0]
    if not os.path.isfile(levelPath):
        return fail( "ERROR: Specified level file doesn't exist: %s\n" % levelPath )

    # Check if the manifest exists, and if it does compare the modification
    # times of the level and the manifest.
    manifestPath = args[1]
    manifestWrite = True
    manifestExists = os.path.isfile(manifestPath)
    if (manifestExists):
        levelTime = os.path.getmtime(levelPath)
        manifestTime = os.path.getmtime(manifestPath)
        manifestWrite = (levelTime > manifestTime)

    # Early out if the manifest is newer than the level - it doesn't need
    # to be regenerated.
    if not manifestWrite:
        return 0

    # Make the directory if it doesn't exist?
    manifestDir = os.path.dirname(manifestPath)
    if manifestDir != '' and not os.path.exists(manifestDir):
        os.makedirs(manifestDir)

    # Open the manifest for writing (and overwrite if exists).
    with open(manifestPath, 'w') as mf:

        # Base name.
        baseName = os.path.basename(levelPath).replace(os.path.splitext(levelPath)[1], '')

        # Suffixes.
        suffixPosition = '_pos.yml'
        suffixInstances = '_inst.yml'

        # Load the level file.
        lvl = LevelEditor.Game.Level( levelPath )

        # Do we have sublevels?
        hasSublevels = 'sublevels' in [f.attrib['name'].lower() for f in lvl.getFolders()]

        # If we do...
        if hasSublevels:
            sublevelsRoot = [f for f in lvl.gameObjectFolder.findall( './gap:folder' ) if f.attrib['name'].lower() == 'sublevels'][0]
            sublevels = sublevelsRoot.findall( './gap:folder' )
            for sublevel in sublevels:
                baseName_sublevel = "%s_%s" % (baseName, sublevel.attrib['name'])
                mf.write("%s.yml\t" % (baseName_sublevel))
                mf.write("%s%s\t" % (baseName_sublevel, suffixPosition))
                mf.write("%s%s\n" % (baseName_sublevel, suffixInstances))
            
        # If we don't...
        else:
            mf.write("%s.yml\t" % (baseName))
            mf.write("%s%s\t" % (baseName, suffixPosition))
            mf.write("%s%s\n" % (baseName, suffixInstances))

    if not os.path.isfile( manifestPath ):
        return fail( "ERROR: Level manifest conversion did not produce expected output: %s\n" % manifestPath )

    return 0

if __name__ == "__main__":
    sys.exit( convert_manifest(args) )
