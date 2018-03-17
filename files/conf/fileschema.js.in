{
    "type": "object",
    "properties": {
	"proc_state": {
	    "enum": ["starting", "running", "media_loaded", "restarting", "stop", "suspended"]
	},
	"media_state": {
	    "enum": ["load", "unload", "play", "pause", "stop"]
	},
	"seek_pos": {
	    "type": "integer"
	},
	"args": {
	    "type": "array"
	},
	"subscription": {
	    "type": "boolean"
	},
	"setPlayRate": {
	    "type": "object",
	    "properties": {
		"playRate": {"type": "number"},
		"audioOutput": {"type": "boolean"}
	    }
	},
	"selectTrack": {
	    "type": "object",
	    "properties": {
		"type": {"type": "string"},
		"index": {"type": "integer"}
	    }
	},
	"setUri": {
	    "type": "object",
	    "properties": {
		"uri": {"type": "string"}
	    }
	},
	"setSubtitleSource": {
	    "type": "object",
	    "properties": {
		"uri": {"type": "string"}
		"preferredEncodings": {"type": "string"}
	    }
	},
	"setSubtitleEnable": {
	    "type": "object",
	    "properties": {
		"enable": {"type": "boolean"}
	    }
	},
	"setSubtitlePosition": {
	    "type": "object",
	    "properties": {
		"position": {"type": "integer"}
	    }
	},
	"setSubtitleSync": {
	    "type": "object",
	    "properties": {
		"sync": {"type": "integer"}
	    }
	},
	"setSubtitleFontSize": {
	    "type": "object",
	    "properties": {
		"fontSize": {"type": "integer"}
	    }
	},
	"setSubtitleColor": {
	    "type": "object",
	    "properties": {
		"color": {"type": "integer"}
	    }
	},
	"setSubtitleEncoding": {
	    "type": "object",
	    "properties": {
		"encoding": {"type": "string"}
	    }
	},
	"setSubtitlePresentationMode": {
	    "type": "object",
	    "properties": {
		"presentationMode": {"type": "string"}
	    }
	},
	"setSubtitleCharacterColor": {
	    "type": "object",
	    "properties": {
		"charColor": {"type": "string"}
	    }
	},
	"setSubtitleCharacterOpacity": {
	    "type": "object",
	    "properties": {
		"charOpacity": {"type": "integer"}
	    }
	},
	"setSubtitleCharacterFontSize": {
	    "type": "object",
	    "properties": {
		"charFontSize": {"type": "string"}
	    }
	},
	"setSubtitleCharacterFont": {
	    "type": "object",
	    "properties": {
		"charFont": {"type": "string"}
	    }
	},
	"setSubtitleBackgroundColor": {
	    "type": "object",
	    "properties": {
		"bgColor": {"type": "string"}
	    }
	},
	"setSubtitleBackgroundOpacity": {
	    "type": "object",
	    "properties": {
		"bgOpacity": {"type": "integer"}
	    }
	},
	"setSubtitleCharacterEdge": {
	    "type": "object",
	    "properties": {
		"charEdgeType": {"type": "string"}
	    }
	},
	"setSubtitleWindowColor": {
	    "type": "object",
	    "properties": {
		"windowColor": {"type": "string"}
	    }
	},
	"setSubtitleWindowOpacity": {
	    "type": "object",
	    "properties": {
		"windowOpacity": {"type": "integer"}
	    }
	},
	"setUpdateInterval": {
	    "type": "object",
	    "properties": {
		"currentTimeInterval": {"type": "integer"},
		"bufferRangeInterval": {"type": "integer"}
	    }
	},
	"setUpdateIntervalKV": {
	    "type": "object",
	    "properties": {
		"key": {"type": "string"},
		"value": {"type": "integer"}
	    }
	},
	"takeSnapshot": {
	    "type": "object",
	    "properties": {
		"location": {"type": "string"},
		"format": {"type": "string"},
		"width": {"type": "integer"},
		"height": {"type": "integer"},
		"pictureQuality": {"type": "integer"}
	    }
	},
	"startRecord": {
	    "type": "object",
	    "properties": {
		"location": {"type": "string"},
		"format": {"type": "string"}
	    }
	},
	"changeResolution": {
	    "type": "object",
	    "properties": {
	    "width": {"type": "integer"},
		"height": {"type": "integer"}
	    }
	},
	"setStreamQuality": {
	    "type": "object",
	    "properties": {
	    "width": {"type": "integer"},
	    "height": {"type": "integer"},
	    "bitRate": {"type": "integer"},
	    "init": {"type": "boolean"}
	    }
	},
	"setProperty": {
	    "type": "object",
	    "properties": {
		"payload": {"type": "string"}
	    }
	},
	"setDescriptiveVideoService": {
	    "type": "object",
	    "properties": {
		"enable": {"type": "boolean"}
	    }
	},
	"loadCompleted": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"unloadCompleted": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"trackSelected": {
	    "type": "object",
	    "properties": {
		"type": {"type": "string"},
		"index": {"type": "integer"},
		"mediaId": {"type": "string"}
	    }
	},
	"playing": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"paused": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"seekDone": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"bufferingStart": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"bufferingEnd": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"bufferRange": {
	    "type": "object",
	    "properties": {
		"beginTime": {"type": "integer"},
		"endTime": {"type": "integer"},
		"remainingTime": {"type": "integer"},
		"percent": {"type": "integer"},
		"mediaId": {"type": "string"}
	    }
	},
	"videoFrame": {
	    "type": "object",
	    "properties": {
		"valid": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	},
	"videoInfo": {
	    "type": "object",
	    "properties": {
		"width": {"type": "integer"},
		"height": {"type": "integer"},
		"aspectRatio": {"type": "string"},
		"frameRate": {"type": "number"},
		"mode3D": {"type": "string"},
		"mediaId": {"type": "string"}
	    }
	},
	"audioInfo": {
	    "type": "object",
	    "properties": {
		"sampleRate": {"type": "number"},
		"channels": {"type": "integer"},
		"mediaId": {"type": "string"}
	    }
	},
	"error": {
	    "type": "object",
	    "properties": {
		"errorCode": {"type": "integer"},
		"errorText": {"type": "string"},
		"mediaId": {"type": "string"}
	    }
	},
	"recordInfo": {
	    "type": "object",
	    "properties": {
		"recordState": {"type": "boolean"},
		"elapsedMiliSecond": {"type": "integer"},
		"bitRate": {"type": "integer"},
		"fileSize": {"type": "integer"},
		"fps": {"type": "integer"},
		"mediaId": {"type": "string"}
	    }
	},
	"serviceName": {
	    "type": "string"
	},
	"returnValue": {
	    "type": "boolean"
	},
	"currentTime": {
	    "type": "object",
	    "properties": {
		"currentTime": {"type": "integer"},
		"mediaId": {"type": "string"}
	    }
	},
	"snapshotDone": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
		}
	},
	"fileGenerated": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
		}
	},
	"endOfStream": {
	    "type": "object",
	    "properties": {
		"state": {"type": "boolean"},
		"mediaId": {"type": "string"}
	    }
	}
    }
}
