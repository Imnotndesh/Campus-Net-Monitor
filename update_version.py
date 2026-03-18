#!/usr/bin/env python3
import re
import sys
import os

def update_version_in_main(version):
    main_cpp_path = "src/main.cpp"
    
    if not os.path.exists(main_cpp_path):
        print(f"Error: {main_cpp_path} not found!")
        return False
    
    with open(main_cpp_path, 'r') as file:
        content = file.read()
    
    pattern1 = r'(Serial\.println\("\(--Campus Monitor Probe )v[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9]+)?( --\)"\);?)'
    replacement1 = f'Serial.println("(--Campus Monitor Probe {version} --)")'
    
    pattern2 = r'(ConfigManager::setFirmwareVersion\(")v[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9]+)?("\);?)'
    replacement2 = f'ConfigManager::setFirmwareVersion("{version}")'
    
    changes_made = False
    
    if re.search(pattern1, content):
        content = re.sub(pattern1, replacement1, content)
        print(f"Updated Serial.println version to {version}")
        changes_made = True
    else:
        print("Could not find Serial.println version pattern in main.cpp")
    
    if re.search(pattern2, content):
        content = re.sub(pattern2, replacement2, content)
        print(f"Updated ConfigManager::setFirmwareVersion to {version}")
        changes_made = True
    else:
        print("Could not find ConfigManager::setFirmwareVersion pattern in main.cpp")
    
    if changes_made:
        with open(main_cpp_path, 'w') as file:
            file.write(content)
        print(f"Successfully updated main.cpp with version {version}")
        return True
    else:
        print("No patterns were updated in main.cpp")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python update_version.py <version>")
        sys.exit(1)
    
    version = sys.argv[1]
    if not version.startswith('v'):
        version = 'v' + version
    
    success = update_version_in_main(version)
    sys.exit(0 if success else 1)