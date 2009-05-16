<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:n1="http://www.w3.org/2005/Atom"
  xmlns:openSearch="http://a9.com/-/spec/opensearchrss/1.0/"
  xmlns:gml="http://www.opengis.net/gml"
  xmlns:georss="http://www.georss.org/georss"
  xmlns:media="http://search.yahoo.com/mrss/"
  xmlns:batch="http://schemas.google.com/gdata/batch"
  xmlns:yt="http://gdata.youtube.com/schemas/2007"
  xmlns:gd="http://schemas.google.com/g/2005">

  <xsl:template match="/">
    <playlist>
      <xsl:for-each select="n1:feed/n1:entry">
        <xsl:variable name="ratings">
          <xsl:apply-templates select="gd:rating"/>
        </xsl:variable>
        <group title="{normalize-space(n1:title)}">
          <xsl:apply-templates select="media:group">
            <xsl:with-param name="ratings">
              <xsl:copy-of select="$ratings"/>
            </xsl:with-param>
          </xsl:apply-templates>
        </group>
      </xsl:for-each>
    </playlist>
  </xsl:template>

  <xsl:template match="node()">
  </xsl:template>

  <xsl:template match="media:group">
    <xsl:param name="ratings"/>
    <xsl:variable name="item_title">
      <xsl:choose>
        <xsl:when test="media:title">
          <xsl:value-of select="normalize-space(media:title)"/>
        </xsl:when>
        <xsl:otherwise>Overview</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="item_url">
      <xsl:apply-templates select="media:player"/>
    </xsl:variable>
    <item title="{$item_title}">
      <smil>
        <head>
          <layout>
            <root-layout width="400" height="300" background-color="#F5F5DC"/>
            <region id="title" left="20" top="10" height="18" right="10"/>
            <region id="image" left="5" top="40" width="130" bottom="30"/>
            <region id="rating" left="15" width="100" height="20" bottom="5"/>
            <region id="text" left="140" top="40" bottom="10" right="10" fit="scroll"/>
          </layout>
          <transition id="fade" dur="0.3" type="fade"/>
        </head>
        <body>
          <par>
            <seq repeatCount="indefinite">
              <xsl:apply-templates select="media:thumbnail"/>
            </seq>
            <a href="{$item_url}" target="top">
              <smilText region="title" textFontWeight="bold" textColor="blue" textFontSize="11">
                <xsl:value-of select="$item_title"/>
              </smilText>
            </a>
            <xsl:copy-of select="$ratings"/>
            <smilText region="text" textFontFamily="serif" textFontSize="11">
              <xsl:value-of select="media:description"/>
            </smilText>
          </par>
        </body>
      </smil>
    </item>
    <xsl:apply-templates select="media:content"/>
  </xsl:template>

  <xsl:template match="media:thumbnail">
    <img region="image" src="{@url}" dur="20" transIn="fade" fill="transition" fit="meet"/>
  </xsl:template>

  <xsl:template match="media:content">
    <item url="{@url}"/>
  </xsl:template>

  <xsl:template match="media:player">
    <xsl:value-of select="@url"/>
  </xsl:template>

  <xsl:template name="svg_star">
    <xsl:param name="avg"/>
    <xsl:param name="nr"/>
    <xsl:variable name="x_pos">
      <xsl:value-of select="10 + 40 * $nr"/>
    </xsl:variable>
    <xsl:variable name="fill">
      <xsl:choose>
        <xsl:when test="$avg > $nr">#ff0000</xsl:when>
        <xsl:otherwise>#C0C0C0</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <path style="stroke:#A0A0A0;stroke-width:2px;stroke-opacity:1;fill:{$fill};" d="M 21.429,23.571 L 10.850,18.213 L 0.439,23.890 L 2.266,12.173 L -6.351,4.026 L 5.357,2.143 L 10.443,-8.569 L 15.852,1.984 L 27.612,3.510 L 19.247,11.916 L 21.429,23.571 z" transform="translate({$x_pos},11)"/>
  </xsl:template>

  <xsl:template match="gd:rating">
    <xsl:variable name="avg">
      <xsl:value-of select="floor(@average) mod 6"/>
    </xsl:variable>
    <img region="rating">
      <svg width="200" height="40">
        <xsl:call-template name="svg_star">
          <xsl:with-param name="avg">
            <xsl:value-of select="$avg"/>
          </xsl:with-param>
          <xsl:with-param name="nr">0</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="svg_star">
          <xsl:with-param name="avg">
            <xsl:value-of select="$avg"/>
          </xsl:with-param>
          <xsl:with-param name="nr">1</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="svg_star">
          <xsl:with-param name="avg">
            <xsl:value-of select="$avg"/>
          </xsl:with-param>
          <xsl:with-param name="nr">2</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="svg_star">
          <xsl:with-param name="avg">
            <xsl:value-of select="$avg"/>
          </xsl:with-param>
          <xsl:with-param name="nr">3</xsl:with-param>
        </xsl:call-template>
        <xsl:call-template name="svg_star">
          <xsl:with-param name="avg">
            <xsl:value-of select="$avg"/>
          </xsl:with-param>
          <xsl:with-param name="nr">4</xsl:with-param>
        </xsl:call-template>
      </svg>
    </img>
  </xsl:template>

</xsl:stylesheet>
