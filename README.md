# gem
Gem is a command line tool for coding with Google's gemini
Actions

To compile on linux.

    git clone https://github.com/openconstruct/gem
    cd gem
    cmake -B build
    cmake --build build



run ./gem listmodels the first time to select a model ( 17 is 2.5 pro)


create <filename> "<description>"
Generates a new file based on a description.
<filename>: The path to the file to create (will be overwritten if it exists).
<description>: A quoted string describing the desired content.
edit <filename> "<instruction>"
Modifies an existing file based on an instruction.
<filename>: The path to the existing file to edit.
<instruction>: A quoted string describing the changes to make.
explain <filename>
Asks the Gemini API to explain the content of an existing file.
<filename>: The path to the existing file to explain.


