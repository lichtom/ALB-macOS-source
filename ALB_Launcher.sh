#!/bin/bash
DIR="$(cd "$(dirname "$0")/../Resources" && pwd)"
open -a Terminal "$DIR/ALB"
osascript -e 'tell application "Terminal" to activate'
