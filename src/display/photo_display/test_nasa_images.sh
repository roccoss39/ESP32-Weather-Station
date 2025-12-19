#!/bin/bash
# NASA Images Availability Tester (Bash version)
# Tests all NASA images from esp32_nasa_ultimate.h
# Respectful to GitHub with proper delays

BASE_URL="https://roccoss39.github.io/nasa.github.io-/nasa-images/"
DELAY=1  # 1 second delay between requests
TIMEOUT=10

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 NASA Images Availability Tester (Bash)${NC}"
echo -e "${BLUE}===========================================${NC}"
echo -e "Base URL: ${BASE_URL}"
echo -e "Delay between requests: ${DELAY}s"
echo -e "Timeout: ${TIMEOUT}s"
echo ""

# Check if esp32_nasa_ultimate.h exists
if [ ! -f "esp32_nasa_ultimate.h" ]; then
    echo -e "${RED}❌ File esp32_nasa_ultimate.h not found!${NC}"
    exit 1
fi

echo -e "${BLUE}📖 Extracting filenames from esp32_nasa_ultimate.h...${NC}"

# Extract filenames using grep and sed
filenames=$(grep -o '{"[^"]*\.jpg"}' esp32_nasa_ultimate.h | sed 's/{"//g' | sed 's/"}//g')

if [ -z "$filenames" ]; then
    echo -e "${RED}❌ No filenames found! Check file format${NC}"
    exit 1
fi

total=$(echo "$filenames" | wc -l)
echo -e "${GREEN}✅ Found $total NASA image filenames${NC}"

# Estimate time
est_minutes=$((total * DELAY / 60))
echo -e "${YELLOW}⏱️ Estimated time: ${est_minutes} minutes${NC}"
echo ""

# Counters
count=0
successful=0
failed=0
failed_list=""

# Test each image
while IFS= read -r filename; do
    count=$((count + 1))
    
    # Truncate filename for display
    display_name=$(echo "$filename" | cut -c1-50)
    printf "${CYAN}🔍 [%4d/%d] %-50s ${NC}" "$count" "$total" "$display_name"
    
    # Test URL with curl
    url="${BASE_URL}${filename}"
    response=$(curl -s -I -m "$TIMEOUT" "$url" 2>/dev/null)
    http_code=$(echo "$response" | head -n1 | cut -d' ' -f2)
    
    if [ "$http_code" = "200" ]; then
        # Get content length if available
        size_bytes=$(echo "$response" | grep -i "content-length:" | cut -d' ' -f2 | tr -d '\r')
        if [ -n "$size_bytes" ]; then
            size_kb=$((size_bytes / 1024))
            echo -e "${GREEN}✅ OK (${size_kb}KB)${NC}"
        else
            echo -e "${GREEN}✅ OK${NC}"
        fi
        successful=$((successful + 1))
    elif [ "$http_code" = "404" ]; then
        echo -e "${RED}❌ NOT FOUND (404)${NC}"
        failed=$((failed + 1))
        failed_list="${failed_list}${filename} - 404 Not Found\n"
    elif [ -z "$http_code" ]; then
        echo -e "${YELLOW}⏰ TIMEOUT${NC}"
        failed=$((failed + 1))
        failed_list="${failed_list}${filename} - Timeout\n"
    else
        echo -e "${YELLOW}⚠️ HTTP ${http_code}${NC}"
        failed=$((failed + 1))
        failed_list="${failed_list}${filename} - HTTP ${http_code}\n"
    fi
    
    # Progress report every 100 images
    if [ $((count % 100)) -eq 0 ]; then
        progress=$((count * 100 / total))
        echo -e "${BLUE}📊 Progress: ${count}/${total} (${progress}%) | Success: ${successful} | Failed: ${failed}${NC}"
    fi
    
    # Respectful delay
    if [ "$count" -lt "$total" ]; then
        sleep "$DELAY"
    fi
    
done <<< "$filenames"

# Calculate success rate
if [ "$total" -gt 0 ]; then
    success_rate=$((successful * 100 / total))
else
    success_rate=0
fi

# Final report
echo ""
echo -e "${WHITE}🏁 TEST COMPLETED${NC}"
echo -e "${WHITE}=================${NC}"
echo -e "Total images tested: $total"
echo -e "Successful: ${GREEN}$successful${NC}"
echo -e "Failed: ${RED}$failed${NC}"

if [ "$success_rate" -gt 95 ]; then
    color=$GREEN
elif [ "$success_rate" -gt 80 ]; then
    color=$YELLOW
else
    color=$RED
fi
echo -e "Success rate: ${color}${success_rate}%${NC}"

# Save failed images if any
if [ "$failed" -gt 0 ]; then
    echo ""
    echo -e "${RED}❌ FAILED IMAGES ($failed):${NC}"
    echo -e "$failed_list" | head -20
    
    echo -e "$failed_list" > failed_images.txt
    echo -e "${YELLOW}💾 Failed images saved to failed_images.txt${NC}"
fi

echo ""
echo -e "${GREEN}✅ Test completed successfully!${NC}"