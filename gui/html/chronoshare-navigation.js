var CHRONOSHARE;

var PAGE; // no default page anymore (no reason to have)
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

        // if (page != PAGE)
        // {
        //     PAGE = page;
        //     PARAMS = vars;
        //     URIPARAMS = aurl[1];

        //     if (CHRONOSHARE) {
        //         CHRONOSHARE.run ();
        //     }
        // }
        // else if (aurl[1] != URIPARAMS)
        // {
        //     PARAMS = vars;
        //     URIPARAMS = aurl[1];

        //     if (CHRONOSHARE) {
        //         CHRONOSHARE.run ();
        //     }
        // }

        // this way we can reload by just clicking on the same link
        PAGE = page;
        PARAMS = vars;
        URIPARAMS = aurl[1];

        if (CHRONOSHARE) {
            CHRONOSHARE.run ();
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

    $("#reload-button").click (function() {
        nav_anchor (window.location.href);
    });
});

