#!/bin/bash

echo "Content-type: text/plain"
echo ""

pipe=/tmp/robotIn
if [[ ! -p $pipe ]]; then
        echo "{err:\"Robot Script is not running\"}"
        exit 0
fi

# If no search arguments, exit gracefully now.
  if [ -z "$QUERY_STRING" ]; then
        exit 0
  else
     # No looping this time, just extract the data you are looking for with sed:
     XX=`echo "$QUERY_STRING" | sed -n 's/^.*l_speed=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`
     YY=`echo "$QUERY_STRING" | sed -n 's/^.*r_speed=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`
	 ZZ=`echo "$QUERY_STRING" | sed -n 's/^.*lights=\([^&]*\).*$/\1/p' | sed "s/%20/ /g"`
     XX=$(($XX+128))
     YY=$(($YY+128))
     if [[($XX -le 255) && ($YY -le 255)]]; then
        # send vales as char code
        x=$(printf \\$(printf "%o" $XX))
        y=$(printf \\$(printf "%o" $YY))
		z=$(printf \\$(printf "%o" $ZZ))
        echo $x$y$z > $pipe
        echo "{l_speed:"$XX",r_speed:"$YY"}"
      else
        echo "{err:\"wrong input!\"}"
     fi
  fi
exit 0
