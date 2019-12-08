#!/bin/sh

glslangValidator -e main -o composite.spv -V $1

xxd -i composite.spv > $2
