<?xml version="1.0" encoding="ISO-8859-1"?>

<xsl:stylesheet version="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:str="http://exslt.org/strings">

  <xsl:template match="/">
    <playlist>
      <xsl:for-each select="response/payload/asset">
        <group title="{normalize-space(title)}">
          <item title="{normalize-space(title)}">
            <smil>
              <head>
                <layout>
                  <root-layout width="400" height="300" background-color="#F5F5DC"/>
                  <region id="notice" left="180" top="30" bottom="190" right="10" background-color="#101023"/>
                  <region id="desc" left="10" top="120" bottom="10" right="10" fit="scroll"/>
                </layout>
              </head>
              <body>
                <par dur="indefinite">
                  <smilText region="notice" textColor="#F5F5DC">
                    <p textFontWeight="bold" textAlign="center" textFontSize="10">
Notice for links to Blip TV
                    </p>
                    <p textFontSize="9" textFontStyle="italic">
These links are best viewed when saved first.
                    </p>
                  </smilText>
                  <xsl:variable name="text">
                    <xsl:value-of select="description"/>
                  </xsl:variable>
                  <xsl:variable name="text64">
                    <xsl:value-of select="str:encode-uri($text,true())"/>
                  </xsl:variable>
                  <text region="desc" src="data:text/html,{$text64}" fontPtSize="11" mediaBackgroundOpacity="0" fontFace="serif"/>
                </par>
              </body>
            </smil>
          </item>
          <xsl:apply-templates select="mediaList"/>
        </group>
      </xsl:for-each>
    </playlist>
  </xsl:template>

  <xsl:template match="node()">
  </xsl:template>

  <xsl:template match="mediaList">
    <xsl:apply-templates select="media"/>
  </xsl:template>

  <xsl:template match="media">
    <xsl:variable name="bytes">
      <xsl:value-of select="size"/>
    </xsl:variable>
    <xsl:variable name="size_txt">
      <xsl:choose>
        <xsl:when test="number($bytes) > 1048576">
          <xsl:value-of select="concat(' (',round($bytes div 1048576),' Mb)')"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="concat(' (',round($bytes div 1024), ' kb)')"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:apply-templates select="link">
      <xsl:with-param name="filesize">
        <xsl:value-of select="$size_txt"/>
      </xsl:with-param>
    </xsl:apply-templates>
  </xsl:template>

  <xsl:template match="link">
    <xsl:param name="filesize"/>
    <xsl:variable name="item_type">
      <xsl:choose>
        <xsl:when test="@type">
          <xsl:value-of select="normalize-space(@type)"/>
        </xsl:when>
        <xsl:otherwise>
          <xsl:value-of select="@href"/>
        </xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="item_title">
      <xsl:value-of select="concat($item_type,$filesize)"/>
    </xsl:variable>
    <item title="{$item_title}" url="{@href}"/>
  </xsl:template>

</xsl:stylesheet>
