#!/bin/zsh

cd data
(sort < a | cat b - | tr A-Z a-z > c) 2>> d
