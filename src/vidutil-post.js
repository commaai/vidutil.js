
function copyByteArray(bytes) {
  var heap = bytes.buffer;
  if (heap instanceof ArrayBuffer && typeof heap.slice === 'function') {
    var extract = heap.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
    return new Uint8Array(extract);
  } else {
    return new Uint8Array(bytes);
  }
}

Module.init = function() {
  Module._viddecode_init();
}

Module.decode = function(data, vidFmt, onFrame) {
  var vidFmtBuf = Module._malloc(vidFmt.length + 1);
  Module.stringToUTF8(vidFmt, vidFmtBuf, vidFmt.length + 1);

  var dataBuf = Module._malloc(data.byteLength);
  Module.HEAPU8.set(new Uint8Array(data), dataBuf);

  var frameCbPtr = Module.Runtime.addFunction(function(w, h, ppdata, plinesize) {
    var pdata = [
      Module.getValue(ppdata, '*'),
      Module.getValue(ppdata+4, '*'),
      Module.getValue(ppdata+8, '*'),
      Module.getValue(ppdata+12, '*')
    ];
    var linesize = [
      Module.getValue(plinesize, '*'),
      Module.getValue(plinesize+4, '*'),
      Module.getValue(plinesize+8, '*'),
      Module.getValue(plinesize+12, '*')
    ];
    console.log(linesize);
    var lines = [null, null, null, null];
    for (var i=0; i<4; i++) {
      if (linesize[i]) {
        lines[i] = copyByteArray(Module.HEAPU8.subarray(pdata[i], pdata[i] + linesize[i] * h));
      }
    }
    return onFrame(w, h, lines);
  });

  var ret = Module._viddecode_decode(dataBuf, data.byteLength, vidFmtBuf, frameCbPtr);

  Module.Runtime.removeFunction(frameCbPtr);

  Module._free(dataBuf);
  Module._free(vidFmtBuf);

  return ret;
}
