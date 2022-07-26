#! /usr/bin/env bash
# Grabs thumbnail from given youtube video id, trims it and scales to fill 200x200

TMP_FILENAME="tmp_thumb.jpg"
DEFAULT_FILENAME="thumb.jpg"
DEFAULT_THUMB_TYPE="hqdefault.jpg"
DEFAULT_SCALE="x200^"
FUZZ_PERCENTAGE="15%"

echo -n "YouTube video id: "
read id
echo -n "filename (default: $DEFAULT_FILENAME): "
read filename
echo -n "thumbnail type (default: $DEFAULT_THUMB_TYPE): "
read thumb_type
echo -n "trim? [Y/n]: "
read trim
echo -n "scale (default: $DEFAULT_SCALE): "
read scale

# Set default filename if not given
if [[ -z "$filename" ]]
then
    filename="$DEFAULT_FILENAME"
fi

# Set default thumbnail type if not given
if [[ -z "$thumb_type" ]]
then
    thumb_type="$DEFAULT_THUMB_TYPE"
fi

# Set default scale if not given
if [[ -z "$scale" ]]
then
    scale="$DEFAULT_SCALE"
fi

# Enable/disable trim
trim_arg=""
if [[ -z "$trim" ]] || [[ "$trim" =~ [yY] ]]
then
    trim_arg="-fuzz $FUZZ_PERCENTAGE -trim +repage"
fi

curl "https://img.youtube.com/vi/$id/$thumb_type" -o "$TMP_FILENAME"
eval convert "$TMP_FILENAME" "$trim_arg" -scale "$scale" "$filename"
rm "$TMP_FILENAME"
