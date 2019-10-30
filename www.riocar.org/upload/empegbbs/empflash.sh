#!/bin/sh

IPADDRESS=10.0.0.24
ZIMAGE=zImage
TMPFILE=/tmp/junk

rm -f ${TMPFILE} 2>/dev/null

echo "Sending kernel.."
/usr/bin/ftp ${IPADDRESS} >/dev/null <<-EOF

	put ${ZIMAGE} /dev/flash_kernel
	quit
	EOF

echo "Pausing.."
sleep 2
echo "Readback kernel.."
/usr/bin/ftp ${IPADDRESS} >/dev/null <<-EOF

	get /dev/flash_kernel ${TMPFILE}
	quit
	EOF

KSIZE=`/bin/ls -l ${ZIMAGE} | awk '{print $5;exit}'`

echo -n "Comparing ${KSIZE} bytes.. "
dd if=${TMPFILE} bs=${KSIZE} count=1 2>/dev/null | diff ${ZIMAGE} -

if [ "$?" = "0" ]; then
	echo "Success!"
else
	echo "FAILED ($?) -- try again!"
fi
#rm -f ${TMPFILE} 2>/dev/null
