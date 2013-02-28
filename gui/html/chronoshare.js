
$.Class ("FilesClosure", {}, {
    upcall: function(kind, upcallInfo) {
        $("#loader").fadeOut (500); // ("hidden");
        if (kind == Closure.UPCALL_CONTENT) {
            convertedData = DataUtils.toString (upcallInfo.contentObject.content);
            $("#json").text (convertedData);
            $("#json").removeClass ("hidden");
            data = JSON.parse (convertedData);

            // error handling?
            // if (data.files instanceof Array) {
            //     alert (data.files);
            // }

            var html = $("tbody#files");
            for (var i = 0; i < data.files.length; i++) {
                file = data.files[i];
                html = html.append ([
                    "<tr"+(i%2?" class=\"odd\"":"")+">",
                    "<td class=\"border-left\">" + file.filename + "</td>",
                    "<td>" + file.version + "</td>",
                    "<td>" + file.timestamp + "</td>",
                    "<td class=\"border-right\">" + "</td>",
                    "</tr>"
                ].join());
            }
        }
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#error").html ("Interest timed out");
            $("#error").removeClass ("hidden");
        }
        else {
            $("#error").html ("Unknown error happened");
            $("#error").removeClass ("hidden");
            // some kind of error
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

         this.ndn = new NDN ({host:"127.0.0.1", getHostAndPort: function() { return {host: "127.0.0.1", port: 9696}}});
         // this.ndn.transport.connect (this.ndn);
     },

     run: function () {
         request = new Name ().add (this.files)./*add (folder_in_question).*/add ("nonce").addSegment (0);
         console.log (request.to_uri ());
         $("#files").empty ();
         // $("#loader").removeClass ("hidden");
         $("#loader").fadeIn (500);
         this.ndn.expressInterest (request, new FilesClosure ());
     }
// ,

     // sendRequest: function () {
     //     alert ('bla');
     // }
 });


// $.Class ("onFiles",
//  {
//  },

//  {
//      upcall: function(kind, upcallInfo, tmp) {
//          if (kind
//      }
//  });

// var AsyncGetClosure = function AsyncGetClosure() {
//     Closure.call(this);
// };

// AsyncGetClosure.prototype.upcall = function(kind, upcallInfo, tmp) {
//     if (kind == Closure.UPCALL_FINAL) {
//         // Do nothing.
//     } else if (kind == Closure.UPCALL_CONTENT) {
//         var content = upcallInfo.contentObject;
// 	var nameStr = content.name.getName().split("/").slice(5,6);

// 	if (nameStr == "prefix") {
// 	    document.getElementById('prefixcontent').innerHTML = DataUtils.toString(content.content);
// 	    prefix();
// 	} else if (nameStr == "link") {
// 	    document.getElementById('linkcontent').innerHTML = DataUtils.toString(content.content);
// 	    link();
// 	} else {
// 	    var data = DataUtils.toString(content.content);
// 	    var obj = jQuery.parseJSON(data);
// 	    document.getElementById("lastupdated").innerHTML = obj.lastupdated;
// 	    document.getElementById("lastlog").innerHTML = obj.lastlog;
// 	    document.getElementById("lasttimestamp").innerHTML = obj.lasttimestamp;
// 	}
//     } else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
//         console.log("Closure.upcall called with interest time out.");
//     }
//     return Closure.RESULT_OK;
// };

// function getStatus(name) {
//     // Template interest to get the latest content.
//     var interest = new Interest("/tmp/");
//     interest.childSelector = 1;
//     interest.interestLifetime = 4000;

//     ndn.expressInterest(new Name("/ndn/memphis.edu/netlab/status/" + name), new AsyncGetClosure(), interest);
// }

// // Calls to get the content data.
// function begin() {
//     getStatus("metadata");
//     getStatus("prefix");
//     getStatus("link");
// }

// var ndn;
// $(document).ready(function() {
//     $("#all").fadeIn(500);
//     var res = detect();

//     if (!res) {
// 	$("#base").fadeOut(50);
// 	$("#nosupport").fadeIn(500);
//     } else {
//         //$("#all").fadeIn(500);

//         $.get("test.php", function() {
// 	    openHandle = function() { begin() };
//             ndn = new NDN({host:hostip, onopen:openHandle});
//             ndn.transport.connectWebSocket(ndn);
// 	    $("#base").fadeOut(500, function () {
//                 $("#status").fadeIn(1000);
//             });
//         });
//     }
// });

// $("#continue").click(function() {
//     $("#nosupport").fadeOut(50);
//     $("#base").fadeIn(500);
//     $.get("test.php", function() {
//         openHandle = function() { begin() };
//         ndn = new NDN({host:hostip, onopen:openHandle});
//         ndn.transport.connectWebSocket(ndn);
//         $("#base").fadeOut(500, function () {
//             $("#status").fadeIn(1000);
//         });
//     });
// });
