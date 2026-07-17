#! /usr/bin/env bash

# This script serves as an example of processing several ERA-5 datasets in parallel using haze.
# Since haze doesn't come with multi-processing/multi-threading capabilities on its own,
# external tools like GNU parallel which are widely available need to be leveraged.
#
# The script splits an original log file into equal chunks (i.e. files to process) and
# and merges them afterwards, thus overwriting the file given by `ORIGINAL_LOGFILE`.
# Any messages printed by haze are not redirected to files and appear on the console as
# if haze was started normally.
# Additionally, a tab-separated log file detailing status information of each individual
# process is stored in the directory from which this script is started.
#
# To use the script, several adoptions to your local setup must be made:
#   1. Adapt the file paths point to the AOI, the original logfile you want to process
#      in parallel, the output directory and the number of jobs to run in parallel. These
#      correspond to the variables `AOI`, `ORIGINAL_LOGFILE`, `OUTPUT_DIRECTORY` and
#      `MAX_JOBS`, respectively.
#   2. Adapt the mount options of the Docker command so haze has access to all data.
#   3. Possibly adapt the version tag of haze's docker image to match the most recent one.
#   4. Possbily adapt the `-a` paramter to split when executing with more than 999 CPUs.
#
# Further changes can become necessary depending on your setup!
# 
# Copyright: Florian Katerndahl <florian@katerndahl.com> 2026
# License: GNU GPL v3

set -e

AOI="/data/Dagobah/fonda/grassdata/haze_test/WRS/WRS2_descending_LAND.gpkg"
ORIGINAL_LOGFILE="/home/katerndf/git-repos/haze/abbreviated-logfile"
OUTPUT_DIRECTORY="/data/Dagobah/fonda/grassdata/haze_test_florian/"
MAX_JOBS=30

BNAME=$(basename "$ORIGINAL_LOGFILE")
TEMPDIR=$(mktemp -d)
TEMP_ARG_FILE=$(mktemp -p "${TEMPDIR}")

echo "Temp directory created: " "$TEMPDIR"

# check if original (long) log file exists
if [ ! -f "$ORIGINAL_LOGFILE" ]; then
  echo "$ORIGINAL_LOGFILE" "does not exist" > /dev/stderr
  exit 1
fi

# split orignal file into multiple smaller files
cp "$ORIGINAL_LOGFILE" "$TEMPDIR"
split -d -a 3 -n l/$MAX_JOBS --additional-suffix=.split-log "${TEMPDIR}/${BNAME}" "$TEMPDIR/"

# collect partial files
find "$TEMPDIR" -name "*.split-log" -fprint "${TEMP_ARG_FILE}"

# call haze mulitple times using GNU parallel
parallel --arg-file "${TEMP_ARG_FILE}" -j $MAX_JOBS \
  --halt now,fail=1 --joblog "$PWD"/looming-haze-$(date "+%s").log --keep-order -- \
  docker run --rm -u $(id -u):$(id -g) -v /data:/data -v /tmp:/tmp floriankaterndahl/haze:latest process \
  --wrap-on-edge "$AOI" {} "$OUTPUT_DIRECTORY"

# combine files updated by haze
cat "${TEMPDIR}"/*.split-log > "$ORIGINAL_LOGFILE"

# cleanup
rm -r "$TEMPDIR"
