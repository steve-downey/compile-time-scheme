#!/usr/bin/env python3
import os
import json
import subprocess

def create_godbolt_tree(example_file):
    # Base files to always include
    core_dir = "src/smd/schemepoc/"
    files = []
    
    # Add minimal CMakeLists.txt to glue it
    cmake_lists = f"""cmake_minimum_required(VERSION 3.20)
project(Schemepoc)
add_executable(example {os.path.basename(example_file)})
target_include_directories(example PRIVATE src)
target_compile_options(example PRIVATE -std=c++26 -freflection)
"""
    files.append({"filename": "CMakeLists.txt", "content": cmake_lists})
    
    # Read the target example file
    with open(example_file, 'r') as f:
        files.append({"filename": os.path.basename(example_file), "content": f.read()})
    
    # Read all headers from the project
    for root, _, filenames in os.walk(core_dir):
        for name in filenames:
            if name.endswith('.hpp') or name.endswith('.cpp'):
                path = os.path.join(root, name)
                with open(path, 'r') as f:
                    files.append({
                        "filename": path,
                        "content": f.read()
                    })

    payload = {
        "sessions": [],
        "trees": [
            {
                "id": 1,
                "isCMakeProject": True,
                "compilerLanguageId": "c++",
                "cmakeArgs": "",
                "files": files,
                "compilers": [
                    {
                        "id": "g16",
                        "options": "-std=c++26 -freflection"
                    }
                ]
            }
        ]
    }
    
    with open("godbolt_payload.json", "w") as f:
        json.dump(payload, f)
        
    print(f"Payload created. Uploading to Godbolt API...")
    
    # Use curl to upload because python urllib ssl is acting up
    curl_cmd = [
        "curl", "-s", "-X", "POST",
        "https://godbolt.org/api/shortener",
        "-H", "Content-Type: application/json",
        "-H", "Accept: application/json",
        "-d", "@godbolt_payload.json"
    ]
    
    result = subprocess.run(curl_cmd, capture_output=True, text=True)
    if result.returncode == 0:
        print("Success! Godbolt Project URL:")
        try:
            url_data = json.loads(result.stdout)
            print(url_data.get('url', result.stdout))
        except:
             print(result.stdout)
    else:
        print("Upload failed:", result.stderr)
        
if __name__ == '__main__':
    create_godbolt_tree('src/examples/advanced_reflection_ffi.cpp')

