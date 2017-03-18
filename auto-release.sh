#!/usr/bin/env bash

#######################################
## Attention:
##     Please make sure to update your wscript, RELEASE_NOTES.md and intro.md in
##     docs beforehand, and then run this script to auto-release application.

usage()
{
  echo "Usage:"
  echo "  $0 <VERSION> <IDENTITY> <SPARKLE_KEY>"
  echo ""
  echo "Options:"
  echo "  <VERSION>: the version that will be used for this release"
  echo "  <IDENTITY>: XCode identity (Mac Developer) that will be used for signing the application"
  echo "  <SPARKLE_KEY>: the path to your Sparkle private key for signing the application"
  echo ""
  exit
}

#######################################
## Script for automatically release application binary

VERSION=${VERSION:-$1}
IDENTITY=${IDENTITY:-$2}
KEY_LOCATION=${KEY_LOCATION:-$3}

if [[ -z $VERSION ]] || [[ -z $IDENTITY ]] || [[ -z $KEY_LOCATION ]]; then
  usage
fi

echo "Preparing release $VERSION"
echo "  will sign with XCode identity: $IDENTITY"
echo "  will sign with Sparkle key: $KEY_LOCATION"

BINARY_WEBSERVER=${BINARY_WEBSERVER:-named-data.net:binaries/ChronoShare/}

rm -rf build/release
mkdir build/release

#######################################
## Build .dmg file with code sign with Apple Developer ID

echo "[auto-release] Build .dmg file and sign with Apple Developer ID"

./make-osx-bundle.py -r "${VERSION}" --codesign="${IDENTITY}"

#######################################
## Code sign with Sparkle (private key existed)

cp build/ChronoShare-${VERSION}.dmg build/release/
cp build/release-notes-${VERSION}.html build/release/

#######################################
## Code sign with Sparkle (private key needed)

#OPENSSL="/usr/bin/openssl"
#openssl gendsa <($OPENSSL dsaparam 4096) -out dsa_priv.pem
#chmod 0400 dsa_priv.pem
#openssl dsa -in dsa_priv.pem -pubout -out ndn_sparkle_pub.pem
#mv ndn_sparkle_pub.pem ../res/
#./bin/sign_update "../NDN-${VERSION}.dmg" "${KEY_LOCATION}"

#######################################
## Generate appcast xml file

echo "[auto-release] Generate appcast xml file"

./build/Sparkle/bin/generate_appcast "${KEY_LOCATION}" build/release/

cp sparkle-appcast.xml build/
cat <<EOF | python -
import xml.etree.ElementTree
cast = xml.etree.ElementTree.parse('build/sparkle-appcast.xml')
item = xml.etree.ElementTree.parse('build/release/sparkle-appcast.xml')

channel = cast.getroot()[0]

for item in item.getroot().findall('./channel/item'):
    version = item.findall('.//title')[0].text
    item.findall('.//title')[0].text = "Version %s" % version
    notes = xml.etree.ElementTree.Element('ns0:releaseNotesLink')
    notes.text = 'https://named-data.net/binaries/ChronoShare/release-notes-%s.html' % version
    item.append(notes)
    channel.append(item)

cast.write('sparkle-appcast.xml', encoding="utf-8")
EOF

cp sparkle-appcast.xml build/release/

#######################################
## Upload dmg & xml & html to https://named-data.net/binaries/ChronoShare/

echo "[auto-release] Publish dmg xml and html file to website server"

pushd build
pushd release
ln -s "ChronoShare-${VERSION}.dmg" ChronoShare.dmg
ln -s "release-notes-${VERSION}.html" release-notes.html
popd
popd

echo "Ready to upload:"
echo "rsync -avz build/release/* \"${BINARY_WEBSERVER}\""
