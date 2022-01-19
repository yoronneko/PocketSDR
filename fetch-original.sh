#!/bin/bash
# https://qiita.com/xtetsuji/items/555a1ef19ed21ee42873

git fetch origin
git branch | grep -q "\* taka" || git checkout taka
git merge upstream/master

