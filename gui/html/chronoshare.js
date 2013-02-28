// var PAGE = "fileList";
// var PARAMS = [ ];
// var URIPARAMS = "";

// $(document).ready (function () {
//     function nav_anchor (aurl) {
//         aurl = aurl.split('#');
//         if (aurl[1])
//         {
//             aurl_split = aurl[1].split ('?');
//             page = aurl_split[0];

//             vars = [ ];
//             if (aurl_split[1]) {
//                 var hashes = aurl_split[1].slice (aurl_split[1].indexOf ('?') + 1).split ('&');
//                 for(var i = 0; i < hashes.length; i++)
//                 {
//                     hash = hashes[i].split('=');
//                     vars.push(hash[0]);
//                     vars[hash[0]] = hash[1];
//                 }
//             }

//                 // alert (aurl_split[1]);

//             if (page != PAGE)
//             {
//                 alert (page);
//                 PAGE = page;
//                 PARAMS = vars;
//                 URIPARAMS = aurl_split[1];
//                 alert (URIPARAMS);
//             }
//             else if (aurl_split[1] != URIPARAMS)
//             {
//                 alert (aurl_split[1]);
//             }
//         }
//     }

//     nav_anchor (window.location.href);

//     $("a").click (function(){
//         nav_anchor (this.href)
//     });
// });

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
            $("#json").text (convertedData);
            $("#json").removeClass ("hidden");
            data = JSON.parse (convertedData);

            // error handling?

            var html = $("tbody#files");
            for (var i = 0; i < data.files.length; i++) {
                file = data.files[i];

		row = $("<tr />");
		if (i%2) { row.addClass ("odd"); }

                row.bind('mouseenter mouseleave', function() {
                    $(this).toggleClass('highlighted');
                });

                fileHistoryUrl = new Name ()
                    .add (this.chronoshare.actions)
                    .add (file.filename)
                    .add ("nonce")
                    .addSegment (0);
                // row.attr ("history", fileHistoryUrl.to_uri ());
                row.bind('click', function () {
                    // alert (fileHistoryUrl.to_uri ());
                });

		row.append ($("<td />", {"class": "border-left"}).text (file.filename));
		row.append ($("<td />").text (file.version));
		row.append ($("<td />").text (new Date (file.timestamp)));
		row.append ($("<td />")
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


$.Class ("ChronoShare", { },
 {
     init: function (username, foldername) {
         this.username = new Name (username);
         this.files = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("files").add ("folder");

         this.actions = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("actions").add ("file");

         this.restore = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("cmd").add ("restore").add ("file");

         // this.ndn = new NDN ({host:"127.0.0.1", getHostAndPort: function() { return {host: "127.0.0.1", port: 9696}}});
         this.ndn = new NDN ({host:"127.0.0.1"});
     },


     run: function () {
         request = new Name ().add (this.files)./*add (folder_in_question).*/add ("nonce").addSegment (0);
         console.log (request.to_uri ());
         $("#files").empty ();
         $("#loader").fadeIn (500);
         this.ndn.expressInterest (request, new FilesClosure (this));
     }
 });


