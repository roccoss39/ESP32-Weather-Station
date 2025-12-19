#!/usr/bin/env python3
"""
NASA Images Availability Tester
Tests all 1359 NASA images from esp32_nasa_ultimate.h
Respectful to GitHub with proper delays and error handling
"""

import requests
import time
import re
import sys
from urllib.parse import urljoin

# Configuration
BASE_URL = "https://roccoss39.github.io/nasa.github.io-/nasa-images/"
DELAY_BETWEEN_REQUESTS = 1.0  # 1 second delay to be respectful to GitHub
TIMEOUT_SECONDS = 10
MAX_RETRIES = 3

# Colors for terminal output
class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    END = '\033[0m'

def extract_filenames_from_header():
    """Extract all filenames from esp32_nasa_ultimate.h"""
    print(f"{Colors.BLUE}📖 Reading esp32_nasa_ultimate.h...{Colors.END}")
    
    try:
        with open('esp32_nasa_ultimate.h', 'r', encoding='utf-8') as f:
            content = f.read()
    except FileNotFoundError:
        print(f"{Colors.RED}❌ File esp32_nasa_ultimate.h not found!{Colors.END}")
        return []
    
    # Extract filenames from the new optimized format: {"filename.jpg"}
    pattern = r'{"([^"]+\.jpg)"}'
    filenames = re.findall(pattern, content)
    
    print(f"{Colors.GREEN}✅ Found {len(filenames)} NASA image filenames{Colors.END}")
    return filenames

def test_image_url(filename, index, total):
    """Test a single image URL with retries"""
    url = urljoin(BASE_URL, filename)
    
    for attempt in range(MAX_RETRIES):
        try:
            print(f"{Colors.CYAN}🔍 [{index+1:4d}/{total}] {filename[:50]:<50} ", end="", flush=True)
            
            # Make HEAD request (faster than GET, only checks if exists)
            response = requests.head(url, timeout=TIMEOUT_SECONDS, allow_redirects=True)
            
            if response.status_code == 200:
                size_kb = int(response.headers.get('content-length', 0)) // 1024
                print(f"{Colors.GREEN}✅ OK ({size_kb}KB){Colors.END}")
                return True, None
            
            elif response.status_code == 404:
                print(f"{Colors.RED}❌ NOT FOUND (404){Colors.END}")
                return False, "404 Not Found"
            
            else:
                print(f"{Colors.YELLOW}⚠️ HTTP {response.status_code}{Colors.END}")
                if attempt < MAX_RETRIES - 1:
                    print(f"{Colors.YELLOW}   🔄 Retrying in 2s... (attempt {attempt + 2}/{MAX_RETRIES}){Colors.END}")
                    time.sleep(2)
                else:
                    return False, f"HTTP {response.status_code}"
                    
        except requests.exceptions.Timeout:
            print(f"{Colors.YELLOW}⏰ TIMEOUT{Colors.END}")
            if attempt < MAX_RETRIES - 1:
                print(f"{Colors.YELLOW}   🔄 Retrying... (attempt {attempt + 2}/{MAX_RETRIES}){Colors.END}")
                time.sleep(2)
            else:
                return False, "Timeout"
                
        except requests.exceptions.RequestException as e:
            print(f"{Colors.RED}❌ ERROR: {str(e)[:30]}{Colors.END}")
            if attempt < MAX_RETRIES - 1:
                print(f"{Colors.YELLOW}   🔄 Retrying... (attempt {attempt + 2}/{MAX_RETRIES}){Colors.END}")
                time.sleep(2)
            else:
                return False, str(e)
    
    return False, "Max retries exceeded"

def main():
    """Main testing function"""
    print(f"{Colors.BOLD}{Colors.BLUE}")
    print("🚀 NASA Images Availability Tester")
    print("===================================")
    print(f"Base URL: {BASE_URL}")
    print(f"Delay between requests: {DELAY_BETWEEN_REQUESTS}s")
    print(f"Timeout: {TIMEOUT_SECONDS}s")
    print(f"Max retries: {MAX_RETRIES}")
    print(f"{Colors.END}")
    
    # Extract filenames
    filenames = extract_filenames_from_header()
    if not filenames:
        print(f"{Colors.RED}❌ No filenames found! Check esp32_nasa_ultimate.h format{Colors.END}")
        return
    
    # Statistics
    total_images = len(filenames)
    successful = 0
    failed = 0
    failed_list = []
    
    print(f"{Colors.BLUE}🧪 Starting test of {total_images} images...{Colors.END}")
    print(f"{Colors.YELLOW}⏱️ Estimated time: {(total_images * DELAY_BETWEEN_REQUESTS) / 60:.1f} minutes{Colors.END}")
    print()
    
    start_time = time.time()
    
    try:
        for i, filename in enumerate(filenames):
            success, error = test_image_url(filename, i, total_images)
            
            if success:
                successful += 1
            else:
                failed += 1
                failed_list.append((filename, error))
            
            # Progress every 100 images
            if (i + 1) % 100 == 0:
                elapsed = time.time() - start_time
                rate = (i + 1) / elapsed
                eta = (total_images - i - 1) / rate / 60 if rate > 0 else 0
                print(f"{Colors.BLUE}📊 Progress: {i+1}/{total_images} ({(i+1)/total_images*100:.1f}%) | Success: {successful} | Failed: {failed} | ETA: {eta:.1f}m{Colors.END}")
            
            # Respectful delay (except for last image)
            if i < total_images - 1:
                time.sleep(DELAY_BETWEEN_REQUESTS)
                
    except KeyboardInterrupt:
        print(f"\n{Colors.YELLOW}🛑 Test interrupted by user{Colors.END}")
    
    # Final statistics
    elapsed_time = time.time() - start_time
    success_rate = (successful / total_images) * 100 if total_images > 0 else 0
    
    print(f"\n{Colors.BOLD}{Colors.WHITE}")
    print("🏁 TEST COMPLETED")
    print("=================")
    print(f"Total images tested: {total_images}")
    print(f"Successful: {Colors.GREEN}{successful}{Colors.WHITE}")
    print(f"Failed: {Colors.RED}{failed}{Colors.WHITE}")
    print(f"Success rate: {Colors.GREEN if success_rate > 95 else Colors.YELLOW if success_rate > 80 else Colors.RED}{success_rate:.2f}%{Colors.WHITE}")
    print(f"Total time: {elapsed_time/60:.2f} minutes")
    print(f"{Colors.END}")
    
    # Show failed images
    if failed_list:
        print(f"{Colors.RED}❌ FAILED IMAGES ({len(failed_list)}):{Colors.END}")
        for filename, error in failed_list[:20]:  # Show max 20 failed
            print(f"   {filename} - {error}")
        if len(failed_list) > 20:
            print(f"   ... and {len(failed_list) - 20} more")
        print()
        
        # Save failed list to file
        with open('failed_images.txt', 'w') as f:
            f.write("Failed NASA Images:\n")
            f.write("==================\n\n")
            for filename, error in failed_list:
                f.write(f"{filename} - {error}\n")
        print(f"{Colors.YELLOW}💾 Failed images saved to failed_images.txt{Colors.END}")
    
    print(f"{Colors.GREEN}✅ Test completed successfully!{Colors.END}")

if __name__ == "__main__":
    main()