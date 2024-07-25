#!/usr/bin/env python3

import os
import platform
import json
import re

path = '.vscode/c_cpp_properties.json' if os.path.exists('./.vscode/c_cpp_properties.json') else '../.vscode/c_cpp_properties.json'
txt = open(path).read()
# remove comments
txt = re.sub(r'//.*', '', txt)
props = json.loads(txt)

env = props.get('env', {})
env['workspaceFolder'] = '.'

def check_path(s):
    source = s
    # replace env
    for key, value in env.items():
        s = s.replace('${' + key + '}', value)
    # remove globs from the end
    if s.endswith('**'):
        s = s[:-2]
    elif s.endswith('*'):
        s = s[:-1]
    s = os.path.expanduser(s)
    if s == '':
        s = '.'
    print('Check', source, '->', s)
    assert os.path.exists(s), 'Path does not exist: ' + s

# linux, macos or windows:
platform = platform.system().lower()
if platform == 'darwin':
    platform = 'mac'
elif platform == 'windows':
    platform = 'win32'
elif platform == 'linux':
    pass
else:
    raise Exception('Unknown platform: ' + platform)

for configuration in props['configurations']:
    if platform not in configuration['name'].lower():
        print('Skip configuration', configuration['name'])
        continue

    print('Check configuration', configuration['name'])

    for include_path in configuration.get('includePath', []):
        check_path(include_path)

    for forced_include in configuration.get('forcedInclude', []):
        check_path(forced_include)

    for browse in configuration.get('browse', {}).get('path', []):
        check_path(browse)

    if 'compilerPath' in configuration:
        check_path(configuration['compilerPath'])
