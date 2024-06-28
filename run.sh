#!/bin/bash

# Function to run tests
run_tests() {
  command=$1
  test_number=$2
  echo "---------------------------------"
  echo "Running tests for $command"
  echo "---------------------------------"

  # Loop through the input files

  make clean
  make

  # Define the test range based on the test_number argument
  if [ "$test_number" = "all" ]
  then
    range="{1..8}"
  else
    range="$test_number"
  fi

  for i in $(eval echo $range)
  do
    # Define addresses as the path to the input file
    addresses="./test/$i/addresses_$i.txt"

    comparefile="./test/${i}/addresses_${i}_${command}.txt"

    # Run the command on the input file
    ./vm $addresses $command

    # Compare the output with the comparefile
    total_lines=$(wc -l < "$comparefile")
    diff_lines=$(diff -y --suppress-common-lines correct.txt "$comparefile" | wc -l)
    same_lines=$((total_lines - diff_lines))
    
    # Calculate the similarity percentage
    if [ $total_lines -gt 0 ]
    then
      similarity=$((100 * same_lines / total_lines))
    else
      similarity=0
    fi

    if diff -a correct.txt "$comparefile" > /dev/null
    then
      echo "Test $addresses: Passed"
      echo "---------------------------------"
    else
      echo "Test $addresses: Failed"
      echo
      # Create the diffs directory if it doesn't exist
      mkdir -p diffs
      # Define the diff file name
      diff_file="diffs/diff_${comparefile##*/}"
      # Write the differences to the diff file
      diff -a correct.txt "$comparefile" > "$diff_file"
      # Inform that the differences were written to the diff file
      echo "Differences were written to $diff_file"
      echo
      echo "Similarity: $similarity%"
      echo "---------------------------------"
    fi
  done
}

# Check if an argument was provided
if [ $# -eq 0 ]
then
  echo "Please provide an argument. Use 'all' to run all tests or a number to run a specific test."
  exit 1
fi

# Run tests for fifo and lru sequentially
run_tests fifo $1
run_tests lru $1

# Run make clean at the end of the script
make clean

# Credits message
echo "---------------------------------"
echo " Credits to the contributors "
echo " Tests created and validated by: "
echo "---------------------------------"
echo "Eurivaldo Filho"
echo "Claudio Alves"
echo "Guilherme Cardoso"
echo "Pedro Lira"
echo "Igor Wanderley"
echo "Felipe Serpa"
echo "Gabriel Lima"
echo "Bruno Ribeiro"
echo "---------------------------------"
