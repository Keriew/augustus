#!/bin/bash

set -e

mkdir deploy
build_dir="$(pwd)/build"

VERSION=$(cat res/version.txt)
if [[ "$GITHUB_REF" =~ ^refs/tags/v ]]
then
  REPO=release
elif [[ "$GITHUB_REF" == "refs/heads/master" ]]
then
  REPO=development
elif [[ "$GITHUB_REF" =~ ^refs/pull/ ]]
then
  PR_ID=${GITHUB_REF##refs/pull/}
  PR_ID=${PR_ID%%/merge}
  VERSION=pr-$PR_ID-$VERSION
else
  echo "Unknown branch type $GITHUB_REF - skipping upload"
fi

DEPLOY_FILE=
case "$DEPLOY" in
"linux")
  PACKAGE=linux
  DEPLOY_FILE=claudius-$VERSION-linux-x86_64.zip
  cp "${build_dir}/claudius.zip" "deploy/$DEPLOY_FILE"
  ;;
"flatpak")
  PACKAGE=linux-flatpak
  DEPLOY_FILE=claudius-$VERSION-linux.flatpak
  flatpak build-export export repo
  flatpak build-bundle export claudius.flatpak com.github.keriew.claudius --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
  cp claudius.flatpak "deploy/$DEPLOY_FILE"
  ;;
"vita")
  PACKAGE=vita
  DEPLOY_FILE=claudius-$VERSION-vita.vpk
  cp "${build_dir}/claudius.vpk" "deploy/$DEPLOY_FILE"
  ;;
"switch")
  PACKAGE=switch
  DEPLOY_FILE=claudius-$VERSION-switch.nro
  cp "${build_dir}/claudius.nro" "deploy/$DEPLOY_FILE"
  ;;
"appimage")
  PACKAGE=linux-appimage
  DEPLOY_FILE=claudius-$VERSION-linux.AppImage
  cp "${build_dir}/claudius.AppImage" "deploy/$DEPLOY_FILE"
  ;;
"mac")
  PACKAGE=mac
  DEPLOY_FILE=claudius-$VERSION-mac.dmg
  cp "${build_dir}/claudius.dmg" "deploy/$DEPLOY_FILE"
  ;;
"android")
  PACKAGE=android
  if [ -f "${build_dir}/claudius.apk" ]
  then
    DEPLOY_FILE=claudius-$VERSION-android.apk
    cp "${build_dir}/claudius.apk" "deploy/$DEPLOY_FILE"
  fi
  ;;
"emscripten")
  PACKAGE=emscripten
  if [ -f "${build_dir}/claudius.html" ]
  then
    DEPLOY_FILE=claudius-$VERSION-emscripten.html
    cp "${build_dir}/claudius.html" "deploy/$DEPLOY_FILE"
  fi
  ;;
*)
  echo "Unknown deploy type $DEPLOY - skipping upload"
  exit
  ;;
esac

if [ ! -z "$SKIP_UPLOAD" ]
then
  echo "Build is configured to skip deploy - skipping upload"
  exit
fi

if [ -z "$REPO" ] || [ -z "$DEPLOY_FILE" ]
then
  echo "No repo or deploy file found - skipping upload"
  exit
fi

if [ -z "$UPLOAD_TOKEN" ]
then
  echo "No upload token found - skipping upload"
  exit
fi

curl -u "$UPLOAD_TOKEN" -T deploy/$DEPLOY_FILE https://augustus.josecadete.net/upload/$REPO/$PACKAGE/$VERSION/$DEPLOY_FILE
echo "Uploaded. URL: https://augustus.josecadete.net/$REPO.html" 
