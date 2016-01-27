#!/bin/bash

git remote -v
echo "Adding remotes..."
git remote add gitlab git@gitlab.com:2gisqtandroid/2gisqt5android.git
git remote add github git@github.com:2gis/2gisqt5android.git

echo ""
echo "Current remotes:"
git remote -v
echo ""

git fetch --all
