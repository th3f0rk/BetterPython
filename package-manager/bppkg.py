#!/usr/bin/env python3
"""
BetterPython Package Manager
Install, manage, and distribute BetterPython packages
"""

import json
import os
import sys
import urllib.request
import urllib.error
from pathlib import Path
from typing import Dict, List, Optional

REGISTRY_URL = "https://registry.betterpython.org"  # Placeholder
LOCAL_REGISTRY = Path.home() / ".betterpython" / "packages"

class Package:
    def __init__(self, name: str, version: str, description: str, files: List[str]):
        self.name = name
        self.version = version
        self.description = description
        self.files = files
        
class PackageManager:
    """Manage BetterPython packages"""
    
    def __init__(self):
        self.local_registry = LOCAL_REGISTRY
        self.local_registry.mkdir(parents=True, exist_ok=True)
        
    def init(self, name: str):
        """Initialize new package"""
        pkg_file = Path("package.json")
        
        if pkg_file.exists():
            print("Error: package.json already exists")
            return
            
        package = {
            "name": name,
            "version": "0.1.0",
            "description": "",
            "main": "main.bp",
            "author": "",
            "license": "MIT",
            "dependencies": {}
        }
        
        with open(pkg_file, 'w') as f:
            json.dump(package, f, indent=2)
            
        print(f"Initialized {name} package")
        
        # Create main file
        main_file = Path("main.bp")
        if not main_file.exists():
            main_file.write_text('''def main() -> int:
    print("Hello from", name, "package!")
    return 0
''')
            
    def install(self, package_name: str, version: str = "latest"):
        """Install package"""
        print(f"Installing {package_name}@{version}...")
        
        # Check local registry first
        local_path = self.local_registry / package_name / version
        if local_path.exists():
            print(f"Found in local registry: {local_path}")
            self.copy_to_project(local_path)
            return
            
        # Try remote registry
        try:
            url = f"{REGISTRY_URL}/packages/{package_name}/{version}"
            response = urllib.request.urlopen(url)
            data = json.loads(response.read())
            
            # Download and install
            self.download_package(package_name, version, data)
            print(f"Installed {package_name}@{version}")
            
        except urllib.error.URLError:
            print(f"Error: Could not fetch package {package_name}")
            print("Package registry not yet available")
            
    def list_packages(self):
        """List installed packages"""
        if not self.local_registry.exists():
            print("No packages installed")
            return
            
        print("Installed packages:")
        for pkg_dir in self.local_registry.iterdir():
            if pkg_dir.is_dir():
                for version_dir in pkg_dir.iterdir():
                    if version_dir.is_dir():
                        print(f"  {pkg_dir.name}@{version_dir.name}")
                        
    def publish(self):
        """Publish package to registry"""
        pkg_file = Path("package.json")
        
        if not pkg_file.exists():
            print("Error: No package.json found")
            return
            
        with open(pkg_file) as f:
            package = json.load(f)
            
        print(f"Publishing {package['name']}@{package['version']}...")
        print("Note: Package registry not yet available")
        print("Package would be published to:", REGISTRY_URL)
        
    def copy_to_project(self, source: Path):
        """Copy package to current project"""
        dest = Path("packages")
        dest.mkdir(exist_ok=True)
        
        import shutil
        shutil.copytree(source, dest / source.name, dirs_exist_ok=True)
        
    def download_package(self, name: str, version: str, data: Dict):
        """Download package from registry"""
        dest = self.local_registry / name / version
        dest.mkdir(parents=True, exist_ok=True)
        
        # Download files
        for file_url in data.get('files', []):
            filename = Path(file_url).name
            response = urllib.request.urlopen(file_url)
            (dest / filename).write_bytes(response.read())

def main():
    if len(sys.argv) < 2:
        print("Usage: bppkg <command> [args]")
        print("")
        print("Commands:")
        print("  init <name>        Initialize new package")
        print("  install <package>  Install package")
        print("  list               List installed packages")
        print("  publish            Publish package")
        sys.exit(1)
        
    command = sys.argv[1]
    manager = PackageManager()
    
    if command == "init":
        if len(sys.argv) < 3:
            print("Usage: bppkg init <name>")
            sys.exit(1)
        manager.init(sys.argv[2])
        
    elif command == "install":
        if len(sys.argv) < 3:
            print("Usage: bppkg install <package>")
            sys.exit(1)
        manager.install(sys.argv[2])
        
    elif command == "list":
        manager.list_packages()
        
    elif command == "publish":
        manager.publish()
        
    else:
        print(f"Unknown command: {command}")
        sys.exit(1)

if __name__ == '__main__':
    main()
