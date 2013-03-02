$.Class ("ChronoShare", { },
 {
     init: function (username, foldername) {
         $("#folder-name").text (foldername);
         $("#user-name").text (username);

         this.username = new Name (username);
         this.files = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("files").add ("folder");

         this.actions = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("info").add ("actions");

         this.restore = new Name ("/localhost").add (this.username).add ("chronoshare").add (foldername).add ("cmd").add ("restore").add ("file");

         this.ndn = new NDN ({host:"127.0.0.1"});
     },

     run: function () {
         console.log ("RUN page: " + PAGE);
         $("#loader").fadeIn (500);
         $("#error").addClass ("hidden");

         cmd = {};
         if (PAGE == "fileList") {
             cmd = this.info_files (PARAMS.item);
         }
         else if (PAGE == "folderHistory") {
             cmd = this.info_actions ("folder", PARAMS.item);
         }
         else if (PAGE == "fileHistory") {
             cmd = this.info_actions ("file", PARAMS.item);
         }

         if (cmd.request && cmd.callback) {
             console.log (cmd.request.to_uri ());
             this.ndn.expressInterest (cmd.request, cmd.callback);
         }
         else {
             $("#loader").fadeOut (500); // ("hidden");
             $("#content").empty ();
             if (cmd.error) {
                 $("#error").html (cmd.error);
             }
             else {
                 $("#error").html ("Unknown error with " + PAGE);
             }
             $("#error").removeClass ("hidden");
         }
     },

     info_files: function(folder) {
         request = new Name ().add (this.files)./*add (folder_in_question).*/addSegment (PARAMS.offset?PARAMS.offset:0);
         return { request:request, callback: new FilesClosure (this) };
     },

     info_actions: function (type/*"file" or "folder"*/, fileOrFolder /*file or folder name*/) {
         if (type=="file" && !fileOrFolder) {
             return { error: "info_actions: fileOrFolder parameter is missing" };
         }

         request = new Name ().add (this.actions).add (type);
         if (fileOrFolder) {
             request.add (fileOrFolder);
         }
         request.addSegment (PARAMS.offset?PARAMS.offset:0);
         return { request: request, callback: new HistoryClosure (this) };
     },

     cmd_restore_file: function (filename, version, hash) {
         request = new Name ().add (this.restore)
             .add (filename)
             .addSegment (version)
             .add (hash);
         console.log (request.to_uri ());
     }
 });

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

            tbody = $("<tbody />", { "id": "file-list-files" });

            // error handling?
            newcontent = $("<div />", { "id": "content" }).append (
                $("<table />", { "class": "item-list" })
                    .append ($("<thead />")
                             .append ($("<tr />")
                                      .append ($("<th />", { "class": "filename border-left", "scope": "col" }).text ("Filename"))
                                      .append ($("<th />", { "class": "version", "scope": "col" }).text ("Version"))
                                      .append ($("<th />", { "class": "size", "scope": "col" }).text ("Size"))
                                      .append ($("<th />", { "class": "modified", "scope": "col" }).text ("Modified"))
                                      .append ($("<th />", { "class": "modified-by border-right", "scope": "col" }).text ("Modified By"))))
                    .append (tbody)
                    .append ($("<tfoot />")
                             .append ($("<tr />")
                                      .append ($("<td />", { "colspan": "5", "class": "border-right border-left" })))));
            newcontent.hide ();

            for (var i = 0; i < data.files.length; i++) {
                file = data.files[i];

		row = $("<tr />");
		if (i%2) { row.addClass ("odd"); }

                row.bind('mouseenter mouseleave', function() {
                    $(this).toggleClass('highlighted');
                });

                row.attr ("filename", file.filename); //encodeURIComponent(encodeURIComponent(file.filename)));


                row.bind('click', function (e) {
                    url = new HistoryClosure (null).base_url ("fileHistory")
                    url += "&item=" + encodeURIComponent (encodeURIComponent ($(this).attr ("filename")));

                    document.location = url;
                });

		row.append ($("<td />", { "class": "filename border-left" }).text (file.filename));
		row.append ($("<td />", { "class": "version" }).text (file.version));
		row.append ($("<td />", { "class": "size" }).text (SegNumToFileSize (file.segNum)));
		row.append ($("<td />", { "class": "modified" }).text (new Date (file.timestamp)));
		row.append ($("<td />", { "class": "modified-by border-right"})
			    .append ($("<userName />").text (file.owner.userName))
			    .append ($("<seqNo> /").text (file.owner.seqNo)));

		tbody = tbody.append (row);
            }

            // if (!PARAMS.offset || PARAMS.offset==0)
            // {
            $("#content").fadeOut ("fast", function () {
                $(this).replaceWith (newcontent);
                $("#content").fadeIn ("fast");
            });

            self = this; // small "cheat"
            $("#content-nav").fadeOut ("fast", function () {
                $("#content-nav a").hide ();

                if (PARAMS.offset !== undefined || data.more !== undefined) {
                    $("#content-nav").fadeIn ("fast");

                    if (data.more !== undefined) {
                        $("#get-more").show ();

                        $("#get-more").unbind ('click').click (function () {
                            url = self.base_url ();
                            url += "&offset="+data.more;

                            document.location = url;
                        });
                    }
                    if (PARAMS.offset > 0) {
                        $("#get-less").show ();

                        $("#get-less").unbind ('click').click (function () {
                            url = self.base_url ();
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
        }
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#error").html ("Interest timed out");
            $("#error").removeClass ("hidden");
        }
        else {
            $("#error").html ("Unknown error happened");
            $("#error").removeClass ("hidden");
        }
    },

    base_url: function () {
        url = "#fileList"+
            "&user="+encodeURIComponent (encodeURIComponent (PARAMS.user)) +
            "&folder="+encodeURIComponent (encodeURIComponent (PARAMS.folder));
        if (PARAMS.item !== undefined) {
            url += "&item="+encodeURIComponent (encodeURIComponent (PARAMS.item));
        }
        return url;
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

            tbody = $("<tbody />", { "id": "history-list-actions" });

            newcontent = $("<div />", { "id": "content" }).append (
                $("<table />", { "class": "item-list" })
                    .append ($("<thead />")
                             .append ($("<tr />")
                                      .append ($("<th />", { "class": "filename border-left", "scope": "col" }).text ("Filename"))
                                      .append ($("<th />", { "class": "version", "scope": "col" }).text ("Version"))
                                      .append ($("<th />", { "class": "modified", "scope": "col" }).text ("Modified"))
                                      .append ($("<th />", { "class": "modified-by border-right", "scope": "col" }).text ("Modified By"))))
                    .append (tbody)
                    .append ($("<tfoot />")
                             .append ($("<tr />")
                                      .append ($("<td />", { "colspan": "4", "class": "border-right border-left" })))));

            for (var i = 0; i < data.actions.length; i++) {
                action = data.actions[i];

	        row = $("<tr />");
	        if (i%2) { row.addClass ("odd"); }
                if (action.action=="DELETE") { row.addClass ("delete"); }

                row.bind('mouseenter mouseleave', function() {
                    $(this).toggleClass('highlighted');
                });

                // row.attr ("filename", );//encodeURIComponent(encodeURIComponent(action.filename)));
                // row.attr ("version",

                // row.bind('click', function (e) {
                //     url = "#fileHistory";
                //     url += "&item=" + $(this).attr ("filename");
                //     pos = URIPARAMS.indexOf ("&");
                //     if (pos >= 0) {
                //         url += URIPARAMS.substring (pos)
                //     }

                //     document.location = url;
                // });

	        row.append ($("<td />", { "class": "filename border-left" }).text (action.filename));
	        row.append ($("<td />", { "class": "version" }).text (action.version));
	        row.append ($("<td />", { "class": "timestamp" }).text (new Date (action.timestamp)));
	        row.append ($("<td />", { "class": "modified-by border-right" })
	        	    .append ($("<userName />").text (action.id.userName))
	        	    .append ($("<seqNo> /").text (action.id.seqNo)));

	        tbody = tbody.append (row);
            }

            // if (!PARAMS.offset || PARAMS.offset==0)
            // {
            $("#content").fadeOut ("fast", function () {
                $(this).replaceWith (newcontent);
                $("#content").fadeIn ("fast");
            });

            self = this; // small "cheat"
            $("#content-nav").fadeOut ("fast", function () {
                $("#content-nav a").hide ();

                if (PARAMS.offset !== undefined || data.more !== undefined) {
                    $("#content-nav").fadeIn ("fast");

                    if (data.more !== undefined) {
                        $("#get-more").show ();

                        $("#get-more").unbind ('click').click (function () {
                            url = self.base_url (PAGE);
                            url += "&offset="+data.more;

                            document.location = url;
                        });
                    }
                    if (PARAMS.offset > 0) {
                        $("#get-less").show ();

                        $("#get-less").unbind ('click').click (function () {
                            url = self.base_url (PAGE);
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
        }
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#error").html ("Interest timed out");
            $("#error").removeClass ("hidden");
        }
        else {
            $("#error").html ("Unknown error happened");
            $("#error").removeClass ("hidden");
        }
    },

    base_url: function (page) {
        url = "#"+page+
            "&user="+encodeURIComponent (encodeURIComponent (PARAMS.user)) +
            "&folder="+encodeURIComponent (encodeURIComponent (PARAMS.folder));
        if (PARAMS.item !== undefined) {
            url += "&item="+encodeURIComponent (encodeURIComponent (PARAMS.item));
        }
        return url;
    }
});




