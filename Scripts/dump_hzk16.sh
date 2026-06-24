cat ../fonts/fonts_hzk16h | hexdump -v -e '"BYTE(0x" 1/1 "%02X" ")\n"' > ../fonts/fonts_hzk16h.ld
cat ../fonts/fonts_hzk16s | hexdump -v -e '"BYTE(0x" 1/1 "%02X" ")\n"' > ../fonts/fonts_hzk16s.ld