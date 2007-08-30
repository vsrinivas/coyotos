<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE xsl:stylesheet>
<!--
 Transformer from things in the osdoc DTD to HTML.
-->

<!-- When chunking is enabled, the output name specified on the
     command line should be a DIRECTORY name. -->

<!--   xmlns="http://www.w3.org/TR/REC-html40" -->

<xsl:stylesheet
  version ="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns:capdoc="file:/dev/null"
  xmlns:doxygen="file:/dev/null"
  xmlns:doc="file:../../../doctools/DTD/osdoc.dtd"
  exclude-result-prefixes="capdoc doc"
  >

  <xsl:output method="xml"
    encoding="utf-8"
    indent="yes"/>

  <!-- Debugging hooks -->
  <xsl:param name="debug"><xsl:number value="0"/></xsl:param>

  <!-- Section leveling -->
  <xsl:param name="seclevel"><xsl:number value="1"/></xsl:param>
  <xsl:param name="manpages"><xsl:number value="0"/></xsl:param>

  <xsl:template name="emit.allsect.attrs">
    <xsl:if test="$manpages = 1">
      <xsl:attribute name="numbered">no</xsl:attribute>
    </xsl:if>
  </xsl:template>

  <xsl:variable name="sect1">
    <xsl:choose>
      <xsl:when test="$manpages = 1"> <!-- implies seclevel == 0 -->
	<xsl:text>manpage</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 0">
	<xsl:text>chapter</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 1">
	<xsl:text>sect1</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 2">
	<xsl:text>sect2</xsl:text>
      </xsl:when>
      <xsl:otherwise>
	<xsl:message terminate="yes">
	  <xsl:text>Error: seclevel must be between 0 and 2 (inclusive).
</xsl:text>
	</xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:variable>

  <xsl:template name="emit.sect1.attrs">
    <xsl:call-template name="emit.allsect.attrs"/>
  </xsl:template>

  <xsl:variable name="sect2">
    <xsl:choose>
      <xsl:when test="$manpages = 1"> <!-- implies seclevel == 0 -->
	<xsl:text>sect1</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 0">
	<xsl:text>sect1</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 1">
	<xsl:text>sect2</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 2">
	<xsl:text>sect3</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:variable>

  <xsl:template name="emit.sect2.attrs">
    <xsl:call-template name="emit.allsect.attrs"/>
  </xsl:template>

  <xsl:variable name="sect3">
    <xsl:choose>
      <xsl:when test="$manpages = 1"> <!-- implies seclevel == 0 -->
	<xsl:text>sect2</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 0">
	<xsl:text>sect2</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 1">
	<xsl:text>sect3</xsl:text>
      </xsl:when>
      <xsl:when test="$seclevel = 2">
	<xsl:text>sect4</xsl:text>
      </xsl:when>
    </xsl:choose>
  </xsl:variable>

  <xsl:template name="emit.sect3.attrs">
    <xsl:call-template name="emit.allsect.attrs"/>
  </xsl:template>

  <xsl:template match="/obdoc">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="pre" mode="passthrough">
    <programlisting>
      <xsl:apply-templates/>
    </programlisting>
  </xsl:template>

  <xsl:template name="emit.brief.description">
    <xsl:choose>
      <xsl:when test="capdoc:description/capdoc:brief">
	<xsl:apply-templates select="capdoc:description/capdoc:brief"/>
      </xsl:when>
      <xsl:otherwise>
<!-- 	<p><xsl:call-template name="emit.nbsp"/></p> -->
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="capdoc:absinterface|capdoc:interface">
    <xsl:element name="{$sect1}">
      <xsl:call-template name="emit.qname.as.id"/>
      <xsl:call-template name="emit.sect1.attrs"/>
      <title>
	<progident><xsl:apply-templates select="@qname"/></progident>
<!-- 	<xsl:if test="$seclevel > 0"> -->
<!-- 	  <xsl:text> (abstract interface)</xsl:text> -->
<!-- 	</xsl:if> -->
      </title>
      <p>
	<xsl:choose>
	  <xsl:when test="name()='capdoc:absinterface'">
	    <b>Abstract Interface</b>
	  </xsl:when>
	  <xsl:when test="name()='capdoc:interface'">
	    <b>Interface</b>
	  </xsl:when>
	</xsl:choose>
	<xsl:text> </xsl:text>
	<progident><xsl:apply-templates select="@qname"/></progident>
<!-- 	<xsl:if test="capdoc:extends"> -->
<!-- 	  <xsl:text> extends </xsl:text> -->
<!-- 	  <progident> -->
<!-- 	    <xsl:apply-templates select="capdoc:extends[1]/@name"/> -->
<!-- 	  </progident> -->
<!-- 	</xsl:if> -->
      </p>

      <xsl:if test="capdoc:extends">
	<p>
	  <b>Derivation:</b><br/>
	  <xsl:for-each select="capdoc:extends">
	    <!-- Reverse the capdoc:extends nodes, so that we can
	         process them in order from root to leaf -->
	    <xsl:sort select="count(following::capdoc:extends)"
	      data-type="number"/>
	    <tt>
	      <xsl:call-template name="emit.nbsp"/>
	      <xsl:call-template name="emit.nbsp"/>
	      <xsl:for-each select="following::capdoc:extends">
		<xsl:call-template name="emit.nbsp"/>
		<xsl:call-template name="emit.nbsp"/>
	      </xsl:for-each>
	      <progident>
		<xsl:apply-templates select="@qname"/>
	      </progident>
	    </tt>
	    <br/>
	  </xsl:for-each>

	  <!-- Finally, emit the current interface name: -->
	  <tt>
	    <xsl:call-template name="emit.nbsp"/>
	    <xsl:call-template name="emit.nbsp"/>
	    <xsl:for-each select="capdoc:extends">
	      <xsl:call-template name="emit.nbsp"/>
	      <xsl:call-template name="emit.nbsp"/>
	    </xsl:for-each>
	    <b><progident><xsl:apply-templates
	    select="@qname"/></progident></b>
	  </tt>
	</p>
      </xsl:if>

      <xsl:apply-templates select="capdoc:description/capdoc:brief"/>
      <xsl:apply-templates select="capdoc:description/capdoc:full"/>

      <xsl:if test="capdoc:const">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Constants</title>
	  <table latex.center="no" latex.colspec="lllp{{3in}}">
	    <thead>
	      <tr>
		<td><b>Name</b></td>
		<td><b>Type</b></td>
		<td><b>Value</b></td>
		<xsl:if test="*/capdoc:description">
		  <td><b>Description</b></td>
		</xsl:if>
	      </tr>
	    </thead>
	    <tbody>
	      <xsl:apply-templates select="capdoc:const"/>
	    </tbody>
	  </table>
	</xsl:element>
      </xsl:if>

      <xsl:if test="capdoc:typedef">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Type Definitions</title>
	  <deflist>
	    <xsl:apply-templates select="capdoc:typedef"/>
	  </deflist>
	</xsl:element>
      </xsl:if>

      <xsl:if test="capdoc:exception">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Exceptions</title>
	  <deflist>
	    <xsl:apply-templates select="capdoc:exception"/>
	  </deflist>
	</xsl:element>
      </xsl:if>

      <xsl:if test="capdoc:enum">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Enumerations</title>
	  <deflist>
	    <xsl:apply-templates select="capdoc:enum"/>
	  </deflist>
	</xsl:element>
      </xsl:if>

      <xsl:if test="capdoc:struct">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Structures</title>
	  <deflist>
	    <xsl:apply-templates select="capdoc:struct"/>
	  </deflist>
	</xsl:element>
      </xsl:if>

      <xsl:if test="capdoc:operation|capdoc:inhOperation">
	<xsl:element name="{$sect2}">
	  <xsl:call-template name="emit.sect2.attrs"/>
	  <title>Operations</title>
	  <deflist>
	    <xsl:apply-templates select="capdoc:operation|capdoc:inhOperation"/>
	  </deflist>
	</xsl:element>
      </xsl:if>
    </xsl:element>
  </xsl:template>

  <xsl:template match="capdoc:brief">
    <p>
      <xsl:variable name="grandparent" select="../.."/>
      <xsl:choose>
	<xsl:when test="name($grandparent) = 'capdoc:absinterface'">
	  <b>Synopsis:</b><xsl:text> </xsl:text>
	</xsl:when>
	<xsl:when test="name($grandparent) = 'capdoc:interface'">
	  <b>Synopsis:</b><xsl:text> </xsl:text>
	</xsl:when>
      </xsl:choose>
      <xsl:apply-templates mode="passthrough"/>
    </p>
  </xsl:template>

  <xsl:template match="capdoc:full">
    <xsl:apply-templates mode="passthrough"/>
  </xsl:template>

  <xsl:template match="capdoc:idlident" mode="passthrough">
    <progident><xsl:apply-templates/></progident>
  </xsl:template>
  <xsl:template match="capdoc:mixident" mode="passthrough">
    <progident><xsl:apply-templates/></progident>
  </xsl:template>

  <xsl:template match="capdoc:typedef">
    <defli>
      <label>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </label>
      <li>
	<xsl:call-template name="emit.brief.description"/>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
	<p>
	  <tt>
	    <xsl:text>typedef </xsl:text>
	    <xsl:apply-templates select="capdoc:type"/>
	    <xsl:text> </xsl:text>
	    <xsl:apply-templates select="@name"/>
	    <xsl:apply-templates select="capdoc:type" mode="post"/>
	    <xsl:text>;</xsl:text>
	  </tt>
	</p>
      </li>
    </defli>
  </xsl:template>

  <xsl:template match="capdoc:exception">
    <defli>
      <label>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </label>
      <li>
	<xsl:call-template name="emit.brief.description"/>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
      </li>
    </defli>
  </xsl:template>

  <xsl:template match="capdoc:enum">
    <defli>
      <label>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </label>
      <li>
	<xsl:call-template name="emit.brief.description"/>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
	<table latex.long="yes" latex.colspec="lllp{{3in}}">
	  <thead>
	    <tr>
	      <td><b>Name</b></td>
	      <td><b>Type</b></td>
	      <td><b>Value</b></td>
	      <xsl:if test="*/capdoc:description">
		<td><b>Description</b></td>
	      </xsl:if>
	    </tr>
	  </thead>
	  <tbody>
	    <xsl:apply-templates select="capdoc:const"/>
	  </tbody>
	</table>
      </li>
    </defli>
  </xsl:template>

  <xsl:template match="capdoc:struct">
    <defli>
      <label>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </label>
      <li>
	<xsl:call-template name="emit.brief.description"/>
	<p>
	  <table latex.colspec="llp{{3in}}">
	    <tbody>
	      <tr>
		<td colspan="3" align="left">
		  <xsl:text>struct </xsl:text>
		  <xsl:apply-templates select="@name"/>
		  <xsl:text>{</xsl:text>
		</td>
	      </tr>
	      <xsl:apply-templates select="capdoc:member"/>
	      <tr>
		<td colspan="3" align="left">
		  <xsl:text>};</xsl:text>
		</td>
	      </tr>
	    </tbody>
	  </table>
	</p>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
      </li>
    </defli>
  </xsl:template>

  <xsl:template match="capdoc:operation|capdoc:inhOperation">
    <defli>
      <label>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </label>
      <li>
	<p>
	  <xsl:if test="name()='capdoc:inhOperation'">
	    <em>inherited from </em>
	    <term><xsl:apply-templates select="@qname"/></term>
	  </xsl:if>
	</p>
	<xsl:call-template name="emit.brief.description"/>
	<p>
	  <tt>
	    <xsl:apply-templates select="capdoc:type"/>
	    <xsl:text> </xsl:text>
	    <xsl:apply-templates select="@name"/>
	    <xsl:text>(</xsl:text>
	    <xsl:apply-templates select="capdoc:formal|capdoc:outformal"/>
	    <xsl:text>);</xsl:text>
	  </tt>
	  <xsl:text>
</xsl:text>
	</p>
	<xsl:if test="capdoc:raises">
	  <p>
	    <b>Raises:</b>
	    <xsl:text> </xsl:text>
	    <xsl:apply-templates select="capdoc:raises/*"/>
	    <!-- hack alert: OSDOC transformers do not handle -->
	    <!-- paragraphs correctly (yet) -->
	    <xsl:text>
</xsl:text>
	  </p>
	</xsl:if>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
      </li>
    </defli>
  </xsl:template>

  <xsl:template match="capdoc:formal|capdoc:outformal">
    <br/>
    <xsl:call-template name="emit.nbsp"/>
    <xsl:call-template name="emit.nbsp"/>
    <xsl:call-template name="emit.nbsp"/>
    <xsl:call-template name="emit.nbsp"/>
    <xsl:if test="name() = 'capdoc:outformal'">
      <xsl:text>OUT </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="capdoc:type"/>
    <xsl:text> </xsl:text>
    <xsl:apply-templates select="@name"/>
    <xsl:apply-templates select="capdoc:type" mode="post"/>
    <xsl:if test="count(following-sibling::capdoc:formal|following-sibling::capdoc:outformal)">
      <xsl:text>,</xsl:text>
    </xsl:if>
  </xsl:template>

  <xsl:template match="capdoc:member">
    <tr>
      <td>
	<xsl:call-template name="emit.nbsp"/>
	<xsl:call-template name="emit.nbsp"/>
	<xsl:call-template name="emit.nbsp"/>
	<xsl:call-template name="emit.nbsp"/>
	<xsl:apply-templates select="capdoc:type"/>
      </td>
      <td>
	<xsl:apply-templates select="@name"/>
	<xsl:apply-templates select="capdoc:type" mode="post"/>
	<xsl:text>;</xsl:text>
      </td>
      <td>
	<xsl:apply-templates select="capdoc:description/capdoc:brief"/>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
      </td>
    </tr>
  </xsl:template>

  <xsl:template match="capdoc:raises/*">
    <progident><xsl:apply-templates select="@name"/></progident>
  </xsl:template>

  <xsl:template match="capdoc:const">
    <tr valign="top">
      <td>
	<term>
	  <xsl:call-template name="emit.qname.as.id"/>
	  <xsl:apply-templates select="@name"/>
	</term>
      </td>
      <td><xsl:apply-templates select="capdoc:type"/></td>
      <td><xsl:apply-templates select="capdoc:literal/@value"/></td>
      <td>
	<xsl:apply-templates select="capdoc:description/capdoc:brief"/>
	<xsl:apply-templates select="capdoc:description/capdoc:full"/>
      </td>
    </tr>
  </xsl:template>

<!--   <xsl:template match="capdoc:*"> -->
<!--     <xsl:element name="{name()}"> -->
<!--       <xsl:apply-templates/> -->
<!--     </xsl:element> -->
<!--   </xsl:template> -->

  <xsl:template match="*" mode="passthrough">
    <xsl:element name="{name()}">
      <xsl:apply-templates mode="passthrough"/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="@*" mode="passthrough">
    <xsl:attribute name="{name()}">
      <xsl:value-of select="."/>
    </xsl:attribute>
  </xsl:template>

  <xsl:template match="doxygen:impl_p" mode="passthrough">
    <p><xsl:apply-templates mode="passthrough"/></p>
  </xsl:template>

  <xsl:template match="doxygen:bug" mode="passthrough">
    <p><b>Bug</b>: <xsl:apply-templates mode="passthrough"/></p>
  </xsl:template>

  <xsl:template match="doxygen:note" mode="passthrough">
    <p><b>Note</b>: <xsl:apply-templates mode="passthrough"/></p>
  </xsl:template>

  <xsl:template match="doxygen:p" mode="passthrough">
    <tt class='parameter'><xsl:apply-templates mode="passthrough"/></tt>
  </xsl:template>

  <xsl:template match="doxygen:em" mode="passthrough">
    <em><xsl:apply-templates mode="passthrough"/></em>
  </xsl:template>

  <xsl:template match="doxygen:b" mode="passthrough">
    <b><xsl:apply-templates mode="passthrough"/></b>
  </xsl:template>

  <xsl:template match="doxygen:c" mode="passthrough">
    <tt><xsl:apply-templates mode="passthrough"/></tt>
  </xsl:template>

  <xsl:template match="table" mode="passthrough">
    <!-- Insert the required TBODY if it is missing -->
    <table>
      <xsl:choose>
	<xsl:when test="./tr">
	  <tbody>
	    <xsl:apply-templates mode="passthrough"/>
	  </tbody>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:apply-templates mode="passthrough"/>
	</xsl:otherwise>
      </xsl:choose>
    </table>
  </xsl:template>

  <xsl:template match="capdoc:type">
    <xsl:call-template name="emit.type.pre"/>
  </xsl:template>
  <xsl:template match="capdoc:type" mode="post">
    <xsl:call-template name="emit.type.post"/>
  </xsl:template>

  <xsl:template name="emit.nbsp">
    <xsl:text disable-output-escaping="yes">&amp;</xsl:text>
    <xsl:text>#x00A0;</xsl:text>
  </xsl:template>

  <xsl:template name="emit.qname.as.id">
    <xsl:attribute name="id">
      <xsl:value-of select="@qname"/>
    </xsl:attribute>
  </xsl:template>

  <xsl:template name="emit.type.pre">
    <xsl:choose>
      <xsl:when test="@tyclass = 'arrayType'">
	<xsl:apply-templates select="capdoc:arrayType/capdoc:type"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:apply-templates select="@ty"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template name="emit.type.post">
    <xsl:choose>
      <xsl:when test="@tyclass = 'arrayType'">
	<xsl:text>[</xsl:text>
	<xsl:apply-templates select="capdoc:arrayType/capdoc:literal/@value"/>
	<xsl:text>]</xsl:text>
      </xsl:when>
      <xsl:otherwise>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

<!-- This is actually the normal catch-all default rule. -->
<!--   <xsl:template match="text()"> -->
<!--     <xsl:value-of select="."/> -->
<!--   </xsl:template> -->

</xsl:stylesheet>
