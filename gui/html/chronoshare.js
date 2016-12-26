info_actions_collection = [];
info_actions_counter = 0;
info_files_collection = [];
info_files_counter = 0;
lastSegmentRequested = -1;
file_data = "";
file_segments = 0;
file_callback = file_onData;
restore_callback = file_onData;

face = null;

$.Class("ChronoShare", {}, {
  init: function(username, foldername) {
    $("#folder-name").text(foldername);
    $("#user-name").text(username);

    this.username = new Name(username);
    this.files = new Name("/localhop")
                   .add(this.username)
                   .add("chronoshare")
                   .add(foldername)
                   .add("info")
                   .add("files")
                   .add("folder");

    this.actions = new Name("/localhop")
                     .add(this.username)
                     .add("chronoshare")
                     .add(foldername)
                     .add("info")
                     .add("actions");

    this.restore = new Name("/localhop")
                     .add(this.username)
                     .add("chronoshare")
                     .add(foldername)
                     .add("cmd")
                     .add("restore")
                     .add("file");


    $("#json").empty();

    if (face == null) {
      var host = "localhost";
      // Connect to the forwarder with a WebSocket.
      face = new Face({host: host});
      console.log("Init Face OK!");
    }
  },

  run: function() {
    console.log("RUN page: " + PAGE);
    $("#loader").fadeIn(500);
    $("#error").addClass("hidden");

    cmd = {};
    if (PAGE == "fileList") {
      info_files_collection = [];
      info_files_counter = 0;
      cmd = this.info_files(PARAMS.item);
    }
    else if (PAGE == "folderHistory") {
      info_actions_collection = [];
      info_actions_counter = 0;
      cmd = this.info_actions("folder", PARAMS.item);
    }
    else if (PAGE == "fileHistory") {
      info_actions_collection = [];
      info_actions_counter = 0;
      cmd = this.info_actions("file", PARAMS.item);
    }
  },

  info_files: function(folder) {
    request = new Name()
                .add(this.files)
                ./*add (folder_in_question).*/ addSegment(
                  PARAMS.offset ? PARAMS.offset : 0);
    face.expressInterest(request, info_files_onData, on_Timeout);
    console.log("Express OK: " + request.to_uri());
  },

  info_actions: function(type /*"file" or "folder"*/,
                         fileOrFolder /*file or folder name*/) {
    if (type == "file" && !fileOrFolder) {
      return {error: "info_actions: fileOrFolder parameter is missing"};
    }

    request = new Name().add(this.actions).add(type);
    if (fileOrFolder) {
      request.add(fileOrFolder);
    }
    request.addSegment(PARAMS.offset ? PARAMS.offset : 0);

    face.expressInterest(request, info_actions_onData, on_Timeout);
    console.log("Express OK: " + request.to_uri());
  },

  cmd_restore_file:
    function(filename, version, hash,
             callback /*function (bool <- data received, status <- returned status)*/) {
      request =
        new Name().add(this.restore).add(filename).addSegment(version).add(hash);
      console.log(request.to_uri());

      restore_callback = callback;
      face.expressInterest(request, restore_onData, on_Timeout);
    },

  get_file:
    function(modifiedBy, hash, segments,
             callback /*function (bool <- data received, data <- returned data)*/) {
      baseName = new Name(modifiedBy).add("chronoshare").add("file").add(hash);

      lastSegmentRequested = 0;
      file_data = "";
      file_segments = segments;
      file_callback = callback;

      request = new Name().add(baseName).addSegment(lastSegmentRequested);
      console.log("Express get_file Name: ", request.to_uri());
      console.log("Total Segments: ", file_segments);
      face.expressInterest(request, file_onData, on_Timeout);

    },

});

function restore_onData(interest, upcallInfo)
{
  convertedData = upcallInfo.getContent().buf().toString('binary');
  console.log("restore_onData");
  console.log(convertedData);
  restore_callback(true, convertedData);
};

function file_onData(interest, upcallInfo)
{
  convertedData = upcallInfo.getContent().buf().toString('binary');
  console.log("file_onData");
  console.log(convertedData);
  file_data += convertedData;
  if (lastSegmentRequested + 1 == file_segments) {
    file_callback(true, file_data);
  }
  else {
    lastSegmentRequested++;
    nextSegment =
      interest.getName().getPrefix(-1).addSegment(lastSegmentRequested);
    face.expressInterest(nextSegment, file_onData, on_Timeout);
  }
};

function info_actions_onData(interest, upcallInfo)
{
  convertedData = upcallInfo.getContent().buf().toString(
    'binary'); //DataUtils.toString (upcallInfo.contentObject.content);
  console.log(convertedData);
  moreName = "more";
  collectionName = "actions";

  if (PARAMS.debug) {
    $("#json").append($(document.createTextNode(convertedData)));
    $("#json").removeClass("hidden");
  }

  console.log("Prefix");
  console.log(interest.getName().getPrefix(-1).to_uri());

  data = JSON.parse(convertedData);
  console.log(data);
  console.log(data[collectionName]);
  var collection = data[collectionName];
  info_actions_collection = info_actions_collection.concat(data[collectionName]);

  console.log(info_actions_collection);

  if (data[moreName] !== undefined) {
    nextSegment = interest.getName().getPrefix(-1).addSegment(data[moreName]);
    info_actions_counter++;

    if (info_actions_counter < 5) {
      console.log("MORE: " + nextSegment.to_uri());
      face.expressInterest(nextSegment, info_actions_onData, on_Timeout);
    }
    else {
      $("#loader").fadeOut(500); // ("hidden");
      var HD = new HistoryDisplay(CHRONOSHARE);
      HD.onData(info_actions_collection, data[moreName]);
    }
  }
  else {
    $("#loader").fadeOut(500); // ("hidden");
    console.log("I'm triggered more undefined~");
    var HD = new HistoryDisplay(CHRONOSHARE);
    HD.onData(info_actions_collection, undefined);
  }
};

function info_files_onData(interest, upcallInfo)
{
  convertedData = upcallInfo.getContent().buf().toString(
    'binary'); //DataUtils.toString (upcallInfo.contentObject.content);
  console.log(convertedData);
  moreName = "more";
  collectionName = "files";

  if (PARAMS.debug) {
    $("#json").append($(document.createTextNode(convertedData)));
    $("#json").removeClass("hidden");
  }

  console.log("Prefix");
  console.log(interest.getName().getPrefix(-1).to_uri());

  data = JSON.parse(convertedData);
  console.log(data);
  console.log(data[collectionName]);
  var collection = data[collectionName];
  info_files_collection = info_files_collection.concat(data[collectionName]);

  console.log(info_files_collection);

  if (data[moreName] !== undefined) {
    nextSegment = interest.getName().getPrefix(-1).addSegment(data[moreName]);
    info_files_counter++;

    if (info_files_counter < 5) {
      console.log("MORE: " + nextSegment.to_uri());
      face.expressInterest(nextSegment, info_files_onData, on_Timeout);
    }
    else {
      $("#loader").fadeOut(500); // ("hidden");
      var HD = new FilesDisplay(CHRONOSHARE);
      HD.onData(info_files_collection, data[moreName]);
    }
  }
  else {
    $("#loader").fadeOut(500); // ("hidden");
    console.log("I'm triggered more undefined~");
    var HD = new FilesDisplay(CHRONOSHARE);
    HD.onData(info_files_collection, undefined);
  }
};


function on_Timeout(interest)
{
  $("#error").html("Ha Ha Interest time out");
  $("#error").removeClass("hidden");
}

$.Class("CmdRestoreFileClosure", {}, {
  init: function(chronoshare, callback) {
    this.chronoshare = chronoshare;
    this.callback = callback;
  },
  upcall: function(kind, upcallInfo) {
    if (kind == Closure.UPCALL_CONTENT ||
        kind == Closure.UPCALL_CONTENT_UNVERIFIED) { //disable content verification
      convertedData = DataUtils.toString(upcallInfo.contentObject.content);
      this.callback(true, convertedData);
    }
    else if (kind == Closure.UPCALL_INTEREST_TIMED_OUT) {
      this.callback(false, "Interest timed out 99");
    }
    else {
      this.callback(false, "Unknown error happened");
    }
  }
});


$.Class("HistoryDisplay", {}, {
  init: function(chronoshare) {
    this.chronoshare = chronoshare;
  },

  previewFile: function(file) {
    if (fileExtension(file.attr("filename")) == "txt") {
      CHRONOSHARE.get_file(file.attr("file_modified_by"),
                           DataUtils.toNumbers(file.attr("file_hash")),
                           file.attr("file_seg_num"),
                           function(status, data) {
                             $("<div />", {
                               "title": "Preview of " + file.attr("filename") +
                                          " version " + file.attr("file_version")
                             })
                               .append($("<pre />").text(data))
                               .dialog({
                                 resizable: true,
                                 width: $(window).width() * 0.8,
                                 maxHeight: $(window).height() * 0.8,
                                 show: "blind",
                                 hide: "fold",
                                 modal: true,
                               });
                           });
    }
    else {
      custom_alert("Preview is not support for this type of file");
    }
  },

  onData: function(data, more) {
    tbody = $("<tbody />", {"id": "history-list-actions"});

    /// @todo Eventually set title for other pages
    $("title").text("ChronoShare - Recent actions" +
                    (PARAMS.item ? " - " + PARAMS.item : ""));

    newcontent =
      $("<div />", {"id": "content"})
        .append($("<h2 />").append($(document.createTextNode("Recent actions ")),
                                   $("<green />").text(PARAMS.item)),
                $("<table />", {"class": "item-list"})
                  .append(
                    $("<thead />")
                      .append($("<tr />")
                                .append($("<th />", {
                                          "class": "filename border-left",
                                          "scope": "col"
                                        }).text("Filename"))
                                .append($("<th />",
                                          {"class": "version", "scope": "col"})
                                          .text("Version"))
                                .append($("<th />",
                                          {"class": "size", "scope": "col"})
                                          .text("Size"))
                                .append($("<th />", {
                                          "class": "modified",
                                          "scope": "col"
                                        }).text("Modified"))
                                .append($("<th />", {
                                          "class": "modified-by border-right",
                                          "scope": "col"
                                        }).text("Modified By"))))
                  .append(tbody)
                  .append($("<tfoot />").append($("<tr />").append($("<td />", {
                    "colspan": "5",
                    "class": "border-right border-left"
                  })))));

    for (var i = 0; i < data.length; i++) {
      action = data[i];

      row = $("<tr />");
      if (i % 2) {
        row.addClass("odd");
      }
      if (action.action == "DELETE") {
        row.addClass("delete");
      }
      else {
        row.addClass("with-context-menu");
        row.attr("file_version", action.version);
        row.attr("file_hash", action.update.hash);
        row.attr("file_seg_num", action.update.segNum);
        row.attr("file_modified_by", action.id.userName);
      }

      row.attr("filename", action.filename);

      self = this;
      if (PARAMS.item != action.filename) {
        row.bind('click', function(e) {
          openHistoryForItem($(this).attr("filename"))
        });
      }
      else {
        row.bind('click', function(e) {
          self.previewFile($(this));
        });
      }

      row.bind('mouseenter mouseleave', function() {
        $(this).toggleClass('highlighted');
      });

      row.append(
        $("<td />", {"class": "filename border-left"})
          .text(action.filename)
          .prepend($("<img />",
                     {"src": imgFullPath(fileExtension(action.filename))})));
      row.append($("<td />", {"class": "version"}).text(action.version));
      row.append(
        $("<td />", {
          "class": "size"
        }).text(action.update ? SegNumToFileSize(action.update.segNum) : ""));
      row.append(
        $("<td />", {"class": "timestamp"})
          .text(new Date(
            action.timestamp +
            "+00:00"))); // conversion from UTC timezone (we store action time in UTC)
      row.append($("<td />", {"class": "modified-by border-right"})
                   .append($("<userName />").text(action.id.userName))
                   .append($("<seqNo> /").text(action.id.seqNo)));

      tbody = tbody.append(row);
    }

    displayContent(newcontent, more, this.base_url(PAGE));

    self = this;
    $.contextMenu('destroy', ".with-context-menu"); // cleanup
    $.contextMenu({
      selector: ".with-context-menu",
      items: {
        "sep1": "---------",
        preview: {
          name: "Preview revision",
          icon: "edit", // ned a better icon
          callback: function(key, opt) {
            self.previewFile(opt.$trigger);
          }
        },
        "sep3": "---------",
        restore: {
          name: "Restore this revision",
          icon: "cut", // need a better icon
          callback: function(key, opt) {
            filename = opt.$trigger.attr("filename");
            version = opt.$trigger.attr("file_version");
            hash = DataUtils.toNumbers(opt.$trigger.attr("file_hash"));
            console.log(hash);
            modified_by = opt.$trigger.attr("file_modified_by");

            $("<div />", {"title": "Restore version " + version + "?"})
              .append(
                $("<p />").append($("<span />", {
                                    "class": "ui-icon ui-icon-alert",
                                    "style": "float: left; margin: 0 7px 50px 0;"
                                  }),
                                  $(document.createTextNode(
                                    "Are you sure you want restore version ")),
                                  $("<green/>").text(version),
                                  $(document.createTextNode(" by ")),
                                  $("<green/>").text(modified_by)))
              .dialog({
                resizable: true,
                height: 200,
                width: 300,
                modal: true,
                show: "blind",
                hide: "fold",
                buttons: {
                  "Restore": function() {
                    self = $(this);
                    CHRONOSHARE.cmd_restore_file(filename, version, hash,
                                                 function(didGetData, response) {
                                                   if (!didGetData ||
                                                       response != "OK") {
                                                     custom_alert(response);
                                                   }

                                                   custom_alert(
                                                     "Restore Succeed!");

                                                   console.log(response);
                                                   self.dialog("close");

                                                   $.timer(function() {
                                                      CHRONOSHARE.run();
                                                    })
                                                     .once(1000);
                                                 });
                  },
                  Cancel: function() {
                    $(this).dialog("close");
                  }
                }
              });
            // openHistoryForItem (opt.$trigger.attr ("filename"));
          }
        },
        "sep2": "---------",
      }
    });
  },

  base_no_item_url: function(page) {
    url = "#" + page + "&user=" +
          encodeURIComponent(encodeURIComponent(PARAMS.user)) + "&folder=" +
          encodeURIComponent(encodeURIComponent(PARAMS.folder));
    return url;
  },

  base_url: function(page) {
    url = "#" + page + "&user=" +
          encodeURIComponent(encodeURIComponent(PARAMS.user)) + "&folder=" +
          encodeURIComponent(encodeURIComponent(PARAMS.folder));
    if (PARAMS.item !== undefined) {
      url += "&item=" + encodeURIComponent(encodeURIComponent(PARAMS.item));
    }
    return url;
  }
});

$.Class("FilesDisplay", {}, {
  init: function(chronoshare) {
    this.chronoshare = chronoshare;
  },

  onData: function(data, more) {
    tbody = $("<tbody />", {"id": "file-list-files"});

    /// @todo Eventually set title for other pages
    $("title").text("ChronoShare - List of files" +
                    (PARAMS.item ? " - " + PARAMS.item : ""));

    // error handling?
    newcontent =
      $("<div />", {"id": "content"})
        .append($("<h2 />").append($(document.createTextNode("List of files ")),
                                   $("<green />").text(PARAMS.item)),
                $("<table />", {"class": "item-list"})
                  .append(
                    $("<thead />")
                      .append($("<tr />")
                                .append($("<th />", {
                                          "class": "filename border-left",
                                          "scope": "col"
                                        }).text("Filename"))
                                .append($("<th />",
                                          {"class": "version", "scope": "col"})
                                          .text("Version"))
                                .append($("<th />",
                                          {"class": "size", "scope": "col"})
                                          .text("Size"))
                                .append($("<th />", {
                                          "class": "modified",
                                          "scope": "col"
                                        }).text("Modified"))
                                .append($("<th />", {
                                          "class": "modified-by border-right",
                                          "scope": "col"
                                        }).text("Modified By"))))
                  .append(tbody)
                  .append($("<tfoot />").append($("<tr />").append($("<td />", {
                    "colspan": "5",
                    "class": "border-right border-left"
                  })))));
    newcontent.hide();

    for (var i = 0; i < data.length; i++) {
      file = data[i];

      row = $("<tr />", {"class": "with-context-menu"});
      if (i % 2) {
        row.addClass("odd");
      }

      row.bind('mouseenter mouseleave', function() {
        $(this).toggleClass('highlighted');
      });

      row.attr("filename",
               file.filename); //encodeURIComponent(encodeURIComponent(file.filename)));
      row.bind('click', function(e) {
        openHistoryForItem($(this).attr("filename"))
      });

      row.append(
        $("<td />", {"class": "filename border-left"})
          .text(file.filename)
          .prepend(
            $("<img />", {"src": imgFullPath(fileExtension(file.filename))})));
      row.append($("<td />", {"class": "version"}).text(file.version));
      row.append(
        $("<td />", {"class": "size"}).text(SegNumToFileSize(file.segNum)));
      row.append($("<td />", {
                   "class": "modified"
                 }).text(new Date(file.timestamp + "+00:00"))); // convert from UTC
      row.append($("<td />", {"class": "modified-by border-right"})
                   .append($("<userName />").text(file.owner.userName))
                   .append($("<seqNo> /").text(file.owner.seqNo)));

      tbody = tbody.append(row);
    }

    displayContent(newcontent, more, this.base_url());

    $.contextMenu('destroy', ".with-context-menu"); // cleanup
    $.contextMenu({
      selector: ".with-context-menu",
      items: {
        "info": {name: "x", type: "html", html: "<b>File operations</b>"},
        "sep1": "---------",
        history: {
          name: "View file history",
          icon: "quit", // need a better icon
          callback: function(key, opt) {
            openHistoryForItem(opt.$trigger.attr("filename"));
          }
        },
      }
    });
  },

  base_url: function() {
    url = "#fileList" +
          "&user=" + encodeURIComponent(encodeURIComponent(PARAMS.user)) +
          "&folder=" + encodeURIComponent(encodeURIComponent(PARAMS.folder));
    if (PARAMS.item !== undefined) {
      url += "&item=" + encodeURIComponent(encodeURIComponent(PARAMS.item));
    }
    return url;
  }
});
