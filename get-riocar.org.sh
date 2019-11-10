#!/bin/bash
# This will download all the files needed for a riocar.org offline copy

RIOCAR_HOST="http://riocar.org"

wget -rk $RIOCAR_HOST
wget -r $RIOCAR_HOST/lens/green_indoor.jpg
wget -r $RIOCAR_HOST/lens/green_ambient.jpg
wget -r $RIOCAR_HOST/lens/green_fullsun.jpg
wget -r $RIOCAR_HOST/lens/blue_indoor.jpg
wget -r $RIOCAR_HOST/lens/blue_ambient.jpg
wget -r $RIOCAR_HOST/lens/blue_fullsun.jpg
wget -r $RIOCAR_HOST/lens/amber_indoor.jpg
wget -r $RIOCAR_HOST/lens/amber_ambient.jpg
wget -r $RIOCAR_HOST/lens/amber_fullsun.jpg
wget -r $RIOCAR_HOST/lens/red_indoor.jpg
wget -r $RIOCAR_HOST/lens/red_ambient.jpg
wget -r $RIOCAR_HOST/lens/red_fullsun.jpg
wget -r $RIOCAR_HOST/lens/clear_indoor.jpg
wget -r $RIOCAR_HOST/lens/clear_ambient.jpg
wget -r $RIOCAR_HOST/lens/clear_fullsun.jpg
wget -r $RIOCAR_HOST/lens/smoke_indoor.jpg
wget -r $RIOCAR_HOST/lens/smoke_ambient.jpg
wget -r $RIOCAR_HOST/lens/smoke_fullsun.jpg
wget -r $RIOCAR_HOST/lens/deepblue_indoor.jpg
wget -r $RIOCAR_HOST/lens/deepblue_ambient.jpg
wget -r $RIOCAR_HOST/lens/deepblue_fullsun.jpg
wget -r $RIOCAR_HOST/lens/lightblue_indoor.jpg
wget -r $RIOCAR_HOST/lens/lightblue_ambient.jpg
wget -r $RIOCAR_HOST/lens/lightblue_fullsun.jpg
wget -r $RIOCAR_HOST/lens/deepred_indoor.jpg
wget -r $RIOCAR_HOST/lens/deepred_ambient.jpg
wget -r $RIOCAR_HOST/lens/deepred_fullsun.jpg
wget -r $RIOCAR_HOST/lens/neonred_indoor.jpg
wget -r $RIOCAR_HOST/lens/neonred_ambient.jpg
wget -r $RIOCAR_HOST/lens/neonred_fullsun.jpg
wget -r $RIOCAR_HOST/lens/deepgreen_indoor.jpg
wget -r $RIOCAR_HOST/lens/deepgreen_ambient.jpg
wget -r $RIOCAR_HOST/lens/deepgreen_fullsun.jpg
wget -r $RIOCAR_HOST/lens/neongreen_indoor.jpg
wget -r $RIOCAR_HOST/lens/neongreen_ambient.jpg
wget -r $RIOCAR_HOST/lens/neongreen_fullsun.jpg
wget -r $RIOCAR_HOST/lens/orange_indoor.jpg
wget -r $RIOCAR_HOST/lens/orange_ambient.jpg
wget -r $RIOCAR_HOST/lens/orange_fullsun.jpg
