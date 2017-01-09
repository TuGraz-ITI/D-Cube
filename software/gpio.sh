if [ "$1" == "0" ] || [ "$1" == "1" ]; then
  
  if [ ! -d /sys/class/gpio/gpio23 ]; then
    echo "23" | sudo tee /sys/class/gpio/export &> /dev/null
  fi

  #remove this check to force toggle 
  if grep -v -q out /sys/class/gpio/gpio23/direction; then
    echo "out" | sudo tee /sys/class/gpio/gpio23/direction > /dev/null
  fi
  echo "$1" | sudo tee /sys/class/gpio/gpio23/value > /dev/null

else
  echo "usage: $0 [0,1]"
fi
