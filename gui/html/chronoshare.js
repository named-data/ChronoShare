var CHRONOSHARE;

var PAGE = "folderHistory";
var PARAMS = [ ];
var URIPARAMS = "";

function nav_anchor (aurl) {
    aurl = aurl.split('#');
    if (aurl[1])
    {
        aurl_split = aurl[1].split ('&');
        page = aurl_split[0];

        vars = [ ];
        for (var i = 1; i < aurl_split.length; i++)
        {
            hash = aurl_split[i].split('=');
            vars.push(hash[0]);
            // there is strange double-encoding problem...
            vars[hash[0]] = decodeURIComponent (decodeURIComponent (hash[1]));
        }

        if (page != PAGE)
        {
            PAGE = page;
            PARAMS = vars;
            URIPARAMS = aurl[1];

            if (CHRONOSHARE) {
                CHRONOSHARE.run ();
            }
        }
        else if (aurl != URIPARAMS)
        {
            PARAMS = vars;
            URIPARAMS = aurl[1];

            if (CHRONOSHARE) {
                CHRONOSHARE.run ();
            }
        }
    }
}

$(document).ready (function () {
    nav_anchor (window.location.href);

    if (!PARAMS.user || !PARAMS.folder)
    {
        $("#error").html ("user and folder must be be specified in the URL");
        $("#error").removeClass ("hidden");
        return;
    }
    else {
        // update in-page URLs
        $(".needs-get-url").each (function (index,element) {
            this.href += "&user="+encodeURIComponent (encodeURIComponent (PARAMS.user))
                + "&folder="+encodeURIComponent (encodeURIComponent (PARAMS.folder));
        });
        $(".needs-get-url").removeClass ("needs-get-url");
    }

    CHRONOSHARE = new ChronoShare (PARAMS.user, PARAMS.folder);
    CHRONOSHARE.run ();

    $(window).on('hashchange', function() {
        nav_anchor (window.location.href);
    });
});

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


$.Class ("FilesClosure", {}, {
    init: function (chronoshare) {
        this.chronoshare = chronoshare;
    },

    upcall: function(kind, upcallInfo) {
        $("#loader").fadeOut (500); // ("hidden");
        if (kind == Closure.UPCALL_CONTENT) {
            convertedData = DataUtils.toString (upcallInfo.contentObject.content);
	    if (PARAMS.debug) {
		$("#json").text (convertedData);
		$("#json").removeClass ("hidden");
	    }
            data = JSON.parse (convertedData);

            // error handling?
            table = $("#content").append (
                $("<table />", { "class": "item-list" })
                    .append ($("<thead />")
                             .append ($("<tr />")
                                      .append ($("<th />", { "class": "filename border-left", "scope": "col" }).text ("Filename"))
                                      .append ($("<th />", { "class": "version", "scope": "col" }).text ("Version"))
                                      .append ($("<th />", { "class": "modified", "scope": "col" }).text ("Modified"))
                                      .append ($("<th />", { "class": "modified-by border-right", "scope": "col" }).text ("Modified By"))))
                    .append ($("<tbody />", { "id": "file-list-files" }))
                    .append ($("<tfoot />")
                             .append ($("<tr />")
                                      .append ($("<td />", { "colspan": "4", "class": "border-right border-left" })))));

            var html = $("#file-list-files");
            for (var i = 0; i < data.files.length; i++) {
                file = data.files[i];

		row = $("<tr />");
		if (i%2) { row.addClass ("odd"); }

                row.bind('mouseenter mouseleave', function() {
                    $(this).toggleClass('highlighted');
                });

                // fileHistoryUrl = new Name ()
                //     .add (this.chronoshare.actions)
                //     .add (file.filename)
                //     .add ("nonce")
                //     .addSegment (0);
                // row.attr ("history", fileHistoryUrl.to_uri ());
                row.bind('click', function () {
                    url = "#fileHistory";
                    url += "&item=" + encodeURIComponent(encodeURIComponent(file.filename));
                    pos = URIPARAMS.indexOf ("&");
                    if (pos >= 0) {
                        url += URIPARAMS.substring (pos)
                    }

                    document.location = url;
                });

		row.append ($("<td />", {"class": "border-left"}).text (file.filename));
		row.append ($("<td />").text (file.version));
		row.append ($("<td />").text (new Date (file.timestamp)));
		row.append ($("<td />", {"class": "border-right"})
			    .append ($("<userName />").text (file.owner.userName))
			    .append ($("<seqNo> /").text (file.owner.seqNo)));

		html = html.append (row);
            }
        }
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#error").html ("Interest timed out");
            $("#error").removeClass ("hidden");
        }
        else {
            $("#error").html ("Unknown error happened");
            $("#error").removeClass ("hidden");
        }
    }
});


$.Class ("HistoryClosure", {}, {
    init: function (chronoshare) {
        this.chronoshare = chronoshare;
    },

    upcall: function(kind, upcallInfo) {
        $("#loader").fadeOut (500); // ("hidden");
        if (kind == Closure.UPCALL_CONTENT) {
            convertedData = DataUtils.toString (upcallInfo.contentObject.content);
	    if (PARAMS.debug) {
		$("#json").text (convertedData);
		$("#json").removeClass ("hidden");
	    }
            data = JSON.parse (convertedData);

            // error handling?
            table = $("#content").append (
                $("<table />", { "class": "item-list" })
                    .append ($("<thead />")
                             .append ($("<tr />")
                                      .append ($("<th />", { "class": "filename border-left", "scope": "col" }).text ("Filename"))
                                      .append ($("<th />", { "class": "version", "scope": "col" }).text ("Version"))
                                      .append ($("<th />", { "class": "modified", "scope": "col" }).text ("Modified"))
                                      .append ($("<th />", { "class": "modified-by border-right", "scope": "col" }).text ("Modified By"))))
                    .append ($("<tbody />", { "id": "history-list-actions" }))
                    .append ($("<tfoot />")
                             .append ($("<tr />")
                                      .append ($("<td />", { "colspan": "4", "class": "border-right border-left" })))));

            var html = $("#history-list-actions");
            for (var i = 0; i < data.actions.length; i++) {
                action = data.actions[i];

	        row = $("<tr />");
	        if (i%2) { row.addClass ("odd"); }

                row.bind('mouseenter mouseleave', function() {
                    $(this).toggleClass('highlighted');
                });

                fileHistoryUrl = new Name ()
                    .add (this.chronoshare.actions)
                    .add (action.filename)
                    .add ("nonce")
                    .addSegment (0);
                // row.attr ("history", fileHistoryUrl.to_uri ());
                row.bind('click', function () {
                    // alert (fileHistoryUrl.to_uri ());
                });

	        row.append ($("<td />", {"class": "border-left"}).text (action.filename));
	        row.append ($("<td />").text (action.version));
	        row.append ($("<td />").text (new Date (action.timestamp)));
	        row.append ($("<td />")
	        	    .append ($("<userName />").text (action.id.userName))
	        	    .append ($("<seqNo> /").text (action.id.seqNo)));

	        html = html.append (row);
            }
        }
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#error").html ("Interest timed out");
            $("#error").removeClass ("hidden");
        }
        else {
            $("#error").html ("Unknown error happened");
            $("#error").removeClass ("hidden");
        }
    }
});


$.Class ("ChronoShare", { },
 {
     init: function (username, foldername) {
         this.username = new Name (username);
         this.files = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("files").add ("folder");

         this.actions = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("actions").add ("folder");

         this.restore = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("cmd").add ("restore").add ("file");

         // this.ndn = new NDN ({host:"127.0.0.1", getHostAndPort: function() { return {host: "127.0.0.1", port: 9696}}});
         this.ndn = new NDN ({host:"127.0.0.1"});
     },


     run: function () {
         $("#content").empty ();
         $("#loader").fadeIn (500);
         $("#error").addClass ("hidden");

         if (PAGE == "fileList") {
             request = new Name ().add (this.files)./*add (folder_in_question).*/add ("nonce").addSegment (0);
             console.log (request.to_uri ());
             this.ndn.expressInterest (request, new FilesClosure (this));
         }
         else if (PAGE == "folderHistory") {
             request = new Name ().add (this.actions)./*add (folder_in_question).*/add ("nonce").addSegment (0);
             console.log (request.to_uri ());
             this.ndn.expressInterest (request, new HistoryClosure (this));
         }
         else if (PAGE == "fileHistory") {
             if (!PARAMS.item) {
                 $("#loader").fadeOut (500); // ("hidden");
                 $("#error").html ("incorrect input for fileHistory command");
                 $("#error").removeClass ("hidden");
                 return;
             }
             request = new Name ().add (this.actions).add (PARAMS.item).add ("nonce").addSegment (0);
             console.log (request.to_uri ());
             this.ndn.expressInterest (request, new HistoryClosure (this));
         }
     }
 });


