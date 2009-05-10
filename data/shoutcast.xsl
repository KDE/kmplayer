<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:template match="/">
    <playlist>
      <xsl:for-each select="stationlist/station">
        <item title="{@name}" url="http://www.shoutcast.com/sbin/tunein-station.pls?id={@id}"/>
      </xsl:for-each>
    </playlist>
  </xsl:template>
</xsl:stylesheet>
