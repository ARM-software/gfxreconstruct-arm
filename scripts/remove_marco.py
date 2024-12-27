#!/usr/bin/env python3

import os
import re

def remove_macros_from_file(file_path, macro_name):
    with open(file_path, 'r') as file:
        code = file.read()

    # Regular expression to match the macro and its content
    pattern = re.compile(r'#ifdef ' + re.escape(macro_name) + r'.*?#endif', re.DOTALL)

    # Remove the macro and its content
    cleaned_code = re.sub(pattern, '', code)

    with open(file_path, 'w') as file:
        file.write(cleaned_code)

def traverse_and_remove_macros(root_dirs, macro_name):
    for root_dir in root_dirs:
        for subdir, _, files in os.walk(root_dir):
            for file in files:
                if file.endswith('.cpp') or file.endswith('.h') or file.endswith('.py'):
                    file_path = os.path.join(subdir, file)
                    remove_macros_from_file(file_path, macro_name)


if '__main__' == __name__:
    root_dirs = ['./framework', './layer']
    macro_name = 'ARM_INTERNAL'
    traverse_and_remove_macros(root_dirs, macro_name)

