# Revert to sequential scan to prevent concurrent brownout
sed -i '' -e 's/pBLEScan->start(90, scanCompleteCB, false);/pBLEScan->clearResults();/' multi_plant_monitor.ino
sed -i '' -e '/void scanCompleteCB/,/}/d' multi_plant_monitor.ino
# Remove the stop() that I added
sed -i '' -e '/pBLEScan->stop();/d' multi_plant_monitor.ino
# Change the background delay back to a sequential BLE scan with a longer recovery delay
sed -i '' -e 's/delay(2000);/delay(3000); \/\/ 3 second battery recovery\n        pBLEScan->start(2, false);\n        pBLEScan->clearResults();/' multi_plant_monitor.ino
