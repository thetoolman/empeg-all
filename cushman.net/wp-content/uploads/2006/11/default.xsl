<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:variable name="allow_commands" select="//playlist/@allow_commands"/>
<xsl:variable name="allow_files" select="//playlist/@allow_files"/>
<xsl:template match="playlist">
<html> 
	<head>
		<title>empeg web lite - <xsl:value-of select="@title"/></title>
		<script type="text/javascript" language="javascript">
		// <![CDATA[
			var FID = "";
			
			function httpGetRequest(url, xml, callback) {
				if (window.XMLHttpRequest)
					var xmlhttp = new XMLHttpRequest();
				else if (window.ActiveXObject)
					var xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
				xmlhttp.open("GET", url, true);
				xmlhttp.onreadystatechange = function() {			
					if(xmlhttp.readyState != 4) return;
					if(callback != null) callback(xml ? xmlhttp.responseXML : xmlhttp.responseText);
				};
				xmlhttp.send(null);
			}
			
			function doCommand(command) {
				httpGetRequest("/?NODATA&" + command, false, null);
				return false;
			}
			
			function insertText(element, text) {
				var the_element = document.getElementById(element);
				var old_text = the_element.firstChild;
				var new_text = document.createTextNode(text);
				if(old_text == null)
					the_element.appendChild(new_text);
				else
					the_element.replaceChild(new_text, old_text);
			}
			
			function itemAttribute(xmldocument, attribute) {
				var the_element = xmldocument.getElementsByTagName("item")[0];
				var the_attribute = the_element.attributes.getNamedItem(attribute);
				return the_attribute.nodeValue;
			}
			
			function getFID(notify) {
				var lines = notify.split('\n');
				for(i = 0; i < lines.length; i++) {
					var line = lines[i].split('=');
					if(line.length == 2) {
						if(line[0].replace(/ +/g, "") == 'notify_FID') {
							var value = line[1].replace(/ +/g, "");
							return value.substring(3, value.length - 3);
						}
					}
				}
			}
			
			function refreshCurrentlyPlaying() {
				httpGetRequest("/proc/empeg_notify", false, checkCurrentlyPlaying);
			}
			
			function checkCurrentlyPlaying(notify) {
				var newFID = getFID(notify);
				if(newFID != FID) {
					FID = newFID;
					httpGetRequest("/?fid=" + FID + "1&ext=.xml", true, writeCurrentlyPlaying);	       
				}
			}
			
			function writeCurrentlyPlaying(xml) {
				insertText("ctitle", itemAttribute(xml, "title"));
				insertText("cartist", itemAttribute(xml, "artist"));
				insertText("csource", itemAttribute(xml, "source"));
				insertText("cyear", "(" + itemAttribute(xml, "year") + ")");
			}
		// ]]>
		</script>
		<style type="text/css">
		/* <![CDATA[ */
			/* --- table styles -------------------------------------------*/
			table			{	font-family: verdana;
							font-size: 13px;
							border: 3px solid #888;
							border-collapse: collapse;	}
			tr.on 			{	background: #e0e0e0		}	
			tr.off 			{	background: #fff;		}
			th 			{	border: 1px solid #888;
							background: #57a;
							color: #ddd;
							padding: 4px;
							text-align: left;		}
			th.title 		{	background: #555;
							color: #ddd;
							font-size: 18px;		}			
			td 			{	border: 1px solid #888;
							padding: 4px;			}
			td.footer 		{	background: #555;
							color: #ddd;
							font-size: 10px;
							padding: 3px 4px 3px 4px;	}
			td.playlist 		{	font-weight: bold;		}
			td.actions 		{	font-size: 10px;
							font-weight: bold;
							background: #bbb;
							color: #777;			}
			tr.off td.actions 	{	background: #ddd;		}
			td.actions span.bullet 	{	margin-left: 4px;
							margin-right: 4px;		}
										
			/* --- misc styles ------------------------------------------- */
			span.control 		{	font-size: 12px;
							padding: 0px 0px 0px 10px;	}
			span#footerlinks	{	float: left;
							text-align: left;		}
			span#tagline		{	float: right;
							text-align: right;		}
										
			/* --- styles for currently playing block -------------------- */
			div.cblock 		{	font-family: verdana;
							font-size: 14px;
							float:left;
							margin-bottom: 14px;		}
			#ctitle 		{	font-size: 18px;		}
			#ctitle_row 		{	padding-bottom: 2px;		}
			#cartist_row 		{	padding-bottom: 2px;		}
			#csource_row 		{	border-top: 1px solid black;
							padding-top: 2px;		}
			#cyear 			{	margin-left: 5px;		}
			#cprev			{	margin-left: 35px;		}
			
			/* --- link styles ------------------------------------------- */
			.actions a,
			.actions a:link,
			.actions a:visited,
			.actions a:active 	{	color: #777;
							text-decoration: none;
							padding: 4px;			}
			.actions a:hover 	{	background-color: #57a;
							text-decoration: none;
							color: #fff;			}
			span.control a,
			span.control a:link,
			span.control a:visited,
			span.control a:active 	{	color: #368;
							font-weight: bold;
							text-decoration: none;
							padding: 2px 3px 4px 3px;	}
			span.control a:hover 	{	background-color: #57a;
							text-decoration: none;
							color: #fff;			}
			a,
			a:link,
			a:visited,
			a:active 		{	color: #368;
							text-decoration: none;
							padding: 2px;			}
			a:hover 		{	background-color: #57a;
							text-decoration: none;
							color: #fff;			}
			#footerlinks a,
			#tagline a,
			#footerlinks a:link,
			#tagline a:link,
			#footerlinks a:visited,
			#tagline a:visited,
			#footerlinks a:active,
			#tagline a:active	{	color: #ddd;
							font-size: 10px;
							text-decoration: none;
							padding: 1px;			}
			#tagline a:hover,
			#footerlinks a:hover 	{	background-color: #ddd;
							text-decoration: none;
							color: #333;			}
		/* ]]> */
		</style>	
	</head>
	<body>
		<div class="cblock">
			<div id="ctitle_row"><span id="ctitle"></span></div>
			<div id="cartist_row"><span id="cartist"></span></div>
			<div id="csource_row"><span id="csource"></span><span id="cyear"></span>
			<xsl:choose><xsl:when test="$allow_commands = 1">
				<span class="control" id="cprev"><a title="Previous Track">
					<xsl:attribute name="onclick">return doCommand('BUTTON=PrevTrack')</xsl:attribute>							<xsl:attribute name="href">/?NODATA&amp;BUTTON=PrevTrack</xsl:attribute>&lt;prev</a></span>
				<span class="control" id="cpause"><a title="Pause">
					<xsl:attribute name="onclick">return doCommand('BUTTON=Pause')</xsl:attribute>
					<xsl:attribute name="href">/?NODATA&amp;BUTTON=Pause</xsl:attribute>pause</a></span>
				<span class="control" id="cnext"><a title="Next Track">
					<xsl:attribute name="onclick">return doCommand('BUTTON=NextTrack')</xsl:attribute>
					<xsl:attribute name="href">/?NODATA&amp;BUTTON=NextTrack</xsl:attribute>next&gt;</a></span>
			</xsl:when></xsl:choose>
			</div>
		</div>		
		<br clear="all"/>
		<table>		
			<thead>
				<tr>
					<th class="title">
						<xsl:attribute name="colspan"><xsl:value-of select="5 + ($allow_commands * 2) + $allow_files"/></xsl:attribute>
						<xsl:value-of select="@title"/>
					</th>
				</tr>
				<tr>
					<xsl:choose><xsl:when test="$allow_commands = 1">
						<th></th>
					</xsl:when></xsl:choose>
					<th>Artist</th>
					<th>Source</th>
					<th>Title</th>
					<th>Track</th>
					<th>Length</th>
					<xsl:choose><xsl:when test="($allow_commands = 1) or ($allow_files = 1)">
						<th><xsl:attribute name="colspan"><xsl:value-of select="$allow_files + $allow_commands"/></xsl:attribute>Actions</th>
					</xsl:when></xsl:choose>
				</tr>
			</thead>
			<tbody>
				<xsl:apply-templates select="items" />
				<tr><td class="footer">
						<xsl:attribute name="colspan"><xsl:value-of select="5 + ($allow_commands * 2) + $allow_files"/></xsl:attribute>
						<span id="footerlinks"><a href="/?FID=101&amp;EXT=.xml">root playlist</a></span><span id="tagline"><a href="http://cushman.net/projects/ewl/">empeg web lite 0.10</a></span>
				</td></tr>
			</tbody>
		</table>
	</body>
	<script type="text/javascript" language="javascript">
	// <![CDATA[
		refreshCurrentlyPlaying();
		setInterval("refreshCurrentlyPlaying()", 1000);
	// ]]>
	</script>
</html>
</xsl:template>
<xsl:template match="items">
	<xsl:for-each select="item">
	<tr>
		<xsl:attribute name="class">
			<xsl:choose>
				<xsl:when test="(position() mod 2 = 0)">on</xsl:when>
				<xsl:when test="(position() mod 2 = 1)">off</xsl:when>
			</xsl:choose>
		</xsl:attribute>
		<xsl:choose>
			<xsl:when test="$allow_commands = 1">
				<td class="actions">
					<a><xsl:attribute name="href">/?NODATA&amp;SERIAL=%23<xsl:value-of select="fid"/></xsl:attribute>
					<xsl:attribute name="onclick">return doCommand('SERIAL=%23<xsl:value-of select="fid"/>')</xsl:attribute>
					<xsl:attribute name="title">Play</xsl:attribute>play</a>
				</td>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="type = 'playlist'">
				<td class="playlist" colspan="4">
					<a><xsl:attribute name="href">/?FID=<xsl:value-of select="tagfid"/>&amp;EXT=.xml</xsl:attribute>
					<xsl:value-of select="title"/></a></td>
				<td><xsl:value-of select="length"/></td>
			</xsl:when>
			<xsl:when test="type = 'tune'">
				<td><xsl:value-of select="artist"/></td>
				<td><xsl:value-of select="source"/></td>
				<td><xsl:value-of select="title"/></td>
				<td><xsl:value-of select="tracknr"/></td>
				<td><xsl:value-of select="duration"/></td>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="$allow_commands = 1">
				<td class="actions">
					<a><xsl:attribute name="href">/?NODATA&amp;SERIAL=%23<xsl:value-of select="fid"/>%2B</xsl:attribute>
					<xsl:attribute name="onclick">return doCommand('SERIAL=%23<xsl:value-of select="fid"/>%2B')</xsl:attribute>
					<xsl:attribute name="title">Append</xsl:attribute>app</a>
					<a><xsl:attribute name="href">/?NODATA&amp;SERIAL=%23<xsl:value-of select="fid"/>!</xsl:attribute>
					<xsl:attribute name="onclick">return doCommand('SERIAL=%23<xsl:value-of select="fid"/>!')</xsl:attribute>
					<xsl:attribute name="title">Insert</xsl:attribute>ins</a>
					<a><xsl:attribute name="href">/?NODATA&amp;SERIAL=%23<xsl:value-of select="fid"/>-</xsl:attribute>
					<xsl:attribute name="onclick">return doCommand('SERIAL=%23<xsl:value-of select="fid"/>-')</xsl:attribute>
					<xsl:attribute name="title">Enqueue</xsl:attribute>enq</a>
				</td>
			</xsl:when>
		</xsl:choose>
		<xsl:choose>
			<xsl:when test="$allow_files = 1">
				<td class="actions">
					<a><xsl:attribute name="href">/<xsl:value-of select="translate(title, ' ', '_')"/>.m3u?FID=<xsl:value-of select="tagfid"/>&amp;EXT=.m3u</xsl:attribute>
						<xsl:attribute name="title">Stream</xsl:attribute>
						<xsl:choose>
							<xsl:when test="$allow_commands = 1">strm</xsl:when>
							<xsl:otherwise>strm</xsl:otherwise>
						</xsl:choose></a> 
					<xsl:choose>
						<xsl:when test="(type = 'tune')">
							<a><xsl:attribute name="href">/<xsl:value-of select="translate(artist, ' ', '_')"/>-<xsl:value-of select="translate(title, ' ', '_')"/>.mp3?FID=<xsl:value-of select="fid"/>&amp;EXT=.mp3</xsl:attribute>
							<xsl:attribute name="title">Save File Locally</xsl:attribute>
							<xsl:choose>
								<xsl:when test="$allow_commands = 1">save</xsl:when>
								<xsl:otherwise>save</xsl:otherwise>
							</xsl:choose></a>
						</xsl:when>
					</xsl:choose>
				</td>
			</xsl:when>
		</xsl:choose>
	</tr>
	</xsl:for-each>
</xsl:template>
</xsl:stylesheet>
