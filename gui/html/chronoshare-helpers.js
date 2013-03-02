function number_format( number, decimals, dec_point, thousands_sep ) {
    // http://kevin.vanzonneveld.net
    // +   original by: Jonas Raoni Soares Silva (http://www.jsfromhell.com)
    // +   improved by: Kevin van Zonneveld (http://kevin.vanzonneveld.net)
    // +     bugfix by: Michael White (http://crestidg.com)
    // +     bugfix by: Benjamin Lupton
    // +     bugfix by: Allan Jensen (http://www.winternet.no)
    // +    revised by: Jonas Raoni Soares Silva (http://www.jsfromhell.com)
    // *     example 1: number_format(1234.5678, 2, '.', '');
    // *     returns 1: 1234.57

    var n = number, c = isNaN(decimals = Math.abs(decimals)) ? 2 : decimals;
    var d = dec_point == undefined ? "," : dec_point;
    var t = thousands_sep == undefined ? "." : thousands_sep, s = n < 0 ? "-" : "";
    var i = parseInt(n = Math.abs(+n || 0).toFixed(c)) + "", j = (j = i.length) > 3 ? j % 3 : 0;

    return s + (j ? i.substr(0, j) + t : "") + i.substr(j).replace(/(\d{3})(?=\d)/g, "$1" + t) + (c ? d + Math.abs(n - i).toFixed(c).slice(2) : "");
}

function SegNumToFileSize (segNum) {
    filesize = segNum * 1024;

    if (filesize >= 1073741824) {
	filesize = number_format(filesize / 1073741824, 2, '.', '') + ' Gb';
    } else {
	if (filesize >= 1048576) {
     	    filesize = number_format(filesize / 1048576, 2, '.', '') + ' Mb';
   	} else {
	    if (filesize > 1024) {
    		filesize = number_format(filesize / 1024, 0) + ' Kb';
  	    } else {
    		filesize = '< 1 Kb';
	    };
 	};
    };
    return filesize;
};

/**
 * @brief Convert binary data represented as non-escaped hex string to Uint8Array
 * @param str String like ba0cb43e4b9639c114a0487d5faa7c70452533963fc8beb37d1b67c09a48a21d
 *
 * Note that if string length is odd, null will be returned
 */
StringHashToUint8Array = function (str) {
    if (str.length % 2 != 0) {
        return null;
    }

    var buf = new Uint8Array (str.length / 2);

    for (var i = 0; i < str.length; i+=2) {
        value = parseInt (str.substring (i, i+2), 16);
        buf[i/2] = value;
    }

    return buf;
};

imgFullPath = function (imgName) {
    return "images/" + imgName + ".png";
}

fileExtension = function (fileName) {
    defaultExtension = "file";
    knownExtensions = ["ai", "aiff", "bib", "bz2", "c", "chm", "conf", "cpp", "css", "csv", "deb", "divx", "doc", "file", "gif", "gz", "hlp", "htm", "html", "iso", "jpeg", "jpg", "js", "mov", "mp3", "mpg", "odc", "odf", "odg", "odi", "odp", "ods", "odt", "ogg", "pdf", "pgp", "php", "pl", "png", "ppt", "pptx", "ps", "py", "ram", "rar", "rb", "rm", "rpm", "rtf", "sql", "swf", "sxc", "sxd", "sxi", "sxw", "tar", "tex", "tgz", "txt", "vcf", "wav", "wma", "wmv", "xls", "xml", "xpi", "xvid", "zip"];

    extStart = fileName.lastIndexOf('.');
    if (extStart < 0) {
	return imgFullPath (defaultExtension);
    }

    extension = fileName.substr (extStart+1);
    // return imgFullPath (extension);
    if ($.inArray(extension, knownExtensions) >= 0) {
    	return imgFullPath (extension);
    }
    else {
    	return imgFullPath (defaultExtension);
    }
};


openHistoryForItem = function (fileName) {
    url = new HistoryClosure (null).base_url ("fileHistory")
    url += "&item=" + encodeURIComponent (encodeURIComponent (fileName));
    document.location = url;
};


displayContent = function (newcontent, more, baseUrl) {

    // if (!PARAMS.offset || PARAMS.offset==0)
    // {
    $("#content").fadeOut ("fast", function () {
        $(this).replaceWith (newcontent);
        $("#content").fadeIn ("fast");
    });

    $("#content-nav").fadeOut ("fast", function () {
        $("#content-nav a").hide ();

        if (PARAMS.offset !== undefined || more !== undefined) {
            $("#content-nav").fadeIn ("fast");

            if (more !== undefined) {
                $("#get-more").show ();

                $("#get-more").unbind ('click').click (function () {
                    url = baseUrl;
                    url += "&offset="+more;

                    document.location = url;
                });
            }
            if (PARAMS.offset > 0) {
                $("#get-less").show ();

                $("#get-less").unbind ('click').click (function () {
                    url = baseUrl;
                    if (PARAMS.offset > 1) {
                        url += "&offset="+(PARAMS.offset - 1);
                    }

                    document.location = url;
                });
            }
        }
    });
    // }
    // else {
    //     tbody.children ().each (function (index, row) {
    //         $("#history-list-actions").append (row);
    //     });
    // }
};


function custom_alert (output_msg, title_msg)
{
    if (!title_msg)
        title_msg = 'Alert';

    if (!output_msg)
        output_msg = 'No Message to Display';

    $("<div></div>").html(output_msg).dialog({
        title: title_msg,
        resizable: false,
        modal: true,
        buttons: {
            "Ok": function()
            {
                $( this ).dialog( "close" );
            }
        }
    });
}