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
	 this.ndn.verify = false; //disable content verification, works WAAAAY faster
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

     cmd_restore_file: function (filename, version, hash, callback/*function (bool <- data received, status <- returned status)*/) {
         request = new Name ().add (this.restore)
             .add (filename)
             .addSegment (version)
             .add (hash);
         console.log (request.to_uri ());
	 this.ndn.expressInterest (request, new CmdRestoreFileClosure (this, callback));
     }
 });

$.Class ("CmdRestoreFileClosure", {}, {
    init: function (chronoshare, callback) {
        this.chronoshare = chronoshare;
	this.callback = callback;
    },
    upcall: function(kind, upcallInfo) {
        if (kind == Closure.UPCALL_CONTENT || kind == Closure.UPCALL_CONTENT_UNVERIFIED) { //disable content verification
            convertedData = DataUtils.toString (upcallInfo.contentObject.content);
	    this.callback (true, convertedData);
	}
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
	    this.callback (false, "Interest timed out");
        }
        else {
	    this.callback (false, "Unknown error happened");
        }
    }
});

$.Class ("RestPipelineClosure", {}, {
    init: function (collectionName, moreName) {
	this.collectionName = collectionName;
	this.moreName = moreName;
	$("#json").empty ();

	this.collection = [];
        this.counter = 0;
    },

    upcall: function(kind, upcallInfo) {
        if (kind == Closure.UPCALL_CONTENT || kind == Closure.UPCALL_CONTENT_UNVERIFIED) { //disable content verification

            convertedData = DataUtils.toString (upcallInfo.contentObject.content);
	    if (PARAMS.debug) {
		$("#json").append ($(document.createTextNode(convertedData)));
		$("#json").removeClass ("hidden");
	    }
            data = JSON.parse (convertedData);

	    this.collection = this.collection.concat (data[this.collectionName]);
	    if (data[this.moreName] !== undefined) {
		nextSegment = upcallInfo.interest.name.cut (1).addSegment (data[this.moreName]);
                this.counter ++;

                if (this.counter < 5) {
		    console.log ("MORE: " +nextSegment.to_uri ());
		    CHRONOSHARE.ndn.expressInterest (nextSegment, this);
                }
                else {
		    $("#loader").fadeOut (500); // ("hidden");
		    this.onData (this.collection, data[this.moreName]);
                }
	    }
	    else {
		$("#loader").fadeOut (500); // ("hidden");
		this.onData (this.collection, undefined);
	    }
	}
        else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
            $("#loader").fadeOut (500); // ("hidden");
	    this.onTimeout (upcallInfo.interest);
        }
        else {
            $("#loader").fadeOut (500); // ("hidden");
	    this.onUnknownError (upcallInfo.interest);
        }

	return Closure.RESULT_OK; // make sure we never re-express the interest
    },

    onData: function(data, more) {
    },

    onTimeout: function () {
        $("#error").html ("Interest timed out");
        $("#error").removeClass ("hidden");
    },

    onUnknownError: function () {
        $("#error").html ("Unknown error happened");
        $("#error").removeClass ("hidden");
    }
});

// $.Class ("FilesClosure", {}, {
RestPipelineClosure ("FilesClosure", {}, {
    init: function (chronoshare) {
	this._super("files", "more");
        this.chronoshare = chronoshare;
    },

    onData: function(data, more) {
        tbody = $("<tbody />", { "id": "file-list-files" });

        /// @todo Eventually set title for other pages
        $("title").text ("ChronoShare - List of files" + (PARAMS.item?" - "+PARAMS.item:""));

        // error handling?
        newcontent = $("<div />", { "id": "content" }).append (
	    $("<h2 />").append ($(document.createTextNode("List of files ")), $("<green />").text (PARAMS.item)),
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

        for (var i = 0; i < data.length; i++) {
            file = data[i];

	    row = $("<tr />", { "class": "with-context-menu" } );
	    if (i%2) { row.addClass ("odd"); }

            row.bind('mouseenter mouseleave', function() {
                $(this).toggleClass('highlighted');
            });

            row.attr ("filename", file.filename); //encodeURIComponent(encodeURIComponent(file.filename)));
            row.bind('click', function (e) { openHistoryForItem ($(this).attr ("filename")) });

	    row.append ($("<td />", { "class": "filename border-left" })
			.text (file.filename)
			.prepend ($("<img />", { "src": fileExtension(file.filename) })));
	    row.append ($("<td />", { "class": "version" }).text (file.version));
	    row.append ($("<td />", { "class": "size" }).text (SegNumToFileSize (file.segNum)));
	    row.append ($("<td />", { "class": "modified" }).text (new Date (file.timestamp+"+00:00"))); // convert from UTC
	    row.append ($("<td />", { "class": "modified-by border-right"})
			.append ($("<userName />").text (file.owner.userName))
			.append ($("<seqNo> /").text (file.owner.seqNo)));

	    tbody = tbody.append (row);
        }

        displayContent (newcontent, more, this.base_url ());

	$.contextMenu( 'destroy',  ".with-context-menu" ); // cleanup
	$.contextMenu({
	    selector: ".with-context-menu",
	    items: {
		"info": {name: "x", type: "html", html: "<b>File operations</b>"},
		"sep1": "---------",
		history: {name: "View file history",
			  icon: "quit", // need a better icon
			  callback: function(key, opt) {
			      openHistoryForItem (opt.$trigger.attr ("filename"));
			  }},
	    }
	});
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


RestPipelineClosure ("HistoryClosure", {}, {
    init: function (chronoshare) {
	this._super("actions", "more");
        this.chronoshare = chronoshare;
    },

    onData: function(data, more) {
        tbody = $("<tbody />", { "id": "history-list-actions" });

        /// @todo Eventually set title for other pages
        $("title").text ("ChronoShare - Recent actions" + (PARAMS.item?" - "+PARAMS.item:""));

        newcontent = $("<div />", { "id": "content" }).append (
	    $("<h2 />").append ($(document.createTextNode("Recent actions ")), $("<green />").text (PARAMS.item)),
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

        for (var i = 0; i < data.length; i++) {
            action = data[i];

	    row = $("<tr />");
	    if (i%2) { row.addClass ("odd"); }
            if (action.action=="DELETE") {
		row.addClass ("delete");
	    }
	    else {
		row.addClass ("with-context-menu");
                row.attr ("file_version", action.version);
		row.attr ("file_hash", action.update.hash);
		row.attr ("file_modified_by", action.id.userName);
	    }

            row.attr ("filename", action.filename);
            row.bind('click', function (e) { openHistoryForItem ($(this).attr ("filename")) });

            row.bind('mouseenter mouseleave', function() {
                $(this).toggleClass('highlighted');
            });

	    row.append ($("<td />", { "class": "filename border-left" })
			.text (action.filename)
			.prepend ($("<img />", { "src": fileExtension(action.filename) })));
	    row.append ($("<td />", { "class": "version" }).text (action.version));
	    row.append ($("<td />", { "class": "size" }).text (action.update?SegNumToFileSize (action.update.segNum):""));
	    row.append ($("<td />", { "class": "timestamp" }).text (new Date (action.timestamp+"+00:00"))); // conversion from UTC timezone (we store action time in UTC)
	    row.append ($("<td />", { "class": "modified-by border-right" })
	        	.append ($("<userName />").text (action.id.userName))
	        	.append ($("<seqNo> /").text (action.id.seqNo)));

	    tbody = tbody.append (row);
        }

        displayContent (newcontent, more, this.base_url (PAGE));

	$.contextMenu( 'destroy',  ".with-context-menu" ); // cleanup
	$.contextMenu({
	    selector: ".with-context-menu",
	    items: {
		"sep1": "---------",
		restore: {name: "Restore this revision",
			  icon: "cut", // need a better icon
			  callback: function(key, opt) {
			      filename = opt.$trigger.attr ("filename");
			      version = opt.$trigger.attr ("file_version");
			      hash = DataUtils.toNumbers (opt.$trigger.attr ("file_hash"));
			      console.log (hash);
			      modified_by = opt.$trigger.attr ("file_modified_by");

			      $("<div />", { "title": "Restore version " + version + "?" })
				  .append ($("<p />")
					   .append ($("<span />", { "class": "ui-icon ui-icon-alert",
								    "style": "float: left; margin: 0 7px 50px 0;" }),
						    $(document.createTextNode ("Are you sure you want restore version ")),
						    $("<green/>").text (version),
						    $(document.createTextNode (" by ")),
						    $("<green/>").text (modified_by)))
				  .dialog ({
				      resizable: true,
				      height:200,
				      width:300,
				      modal: true,
				      buttons: {
					  "Restore": function() {
					      self = $(this);
					      CHRONOSHARE.cmd_restore_file (filename, version, hash, function(didGetData, response) {
						  if (!didGetData || response != "OK") {
						      custom_alert (response);
						  }
						  console.log (response);
						  self.dialog ("close");

						  $.timer (function() {CHRONOSHARE.run ();}).once (1000);
					      });
					  },
					  Cancel: function() {
					      $(this).dialog ("close");
					  }
				      }
				  });
			      // openHistoryForItem (opt.$trigger.attr ("filename"));
			  }},
		"sep2": "---------",
	    }
	});
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




