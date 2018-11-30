v2
  - Initial design

v2.1

  - Add Restrict / Keepout to avoid tracks under Phynode module


<script type="text/javascript" src="https://raw.githubusercontent.com/ofauchon/eagle_viewer/master/src/index.js"></script>
<div id="topBar">
    <form>
        <input type="file" onChange="loadFile(this.files[0])">
    </form>
</div>
<div id="bottomAreaContainer">
    <div id="sideBar">
        <form id="layerCheckboxForm">
        </form>
    </div>
    <div id="scrollView" onMouseDown="MouseDownHandler(event, this)" onMouseMove="MouseMoveHandler(event, this)" onMouseUp="MouseUpHandler(event, this)">
        <svg xmlns="http://www.w3.org/2000/svg" version="1.1" id="svgElementId" style="">
            <style type="text/css">
                body, html {margin: 0; padding: 0}
                .wire, .poly {fill: none; stroke-linecap: round; stroke-linejoin: round}
                .poly {stroke-dasharray: 0.5,3}
                .via {fill-rule: evenodd}
                .rect, .via {stroke: none}
                .origin {stroke: #949494; stroke-width: 1; fill: none; vector-effect: non-scaling-stroke;}
                text {cursor: default}
            </style>
            <style type="text/css" id="styleSheetLayerColors">
            </style>
            <style type="text/css" id="styleSheetLayerVisibility">
            </style>
            <g id="packagesGroup" style="display: none">
            </g>
            <g id="boardGroup">
            </g>
        </svg>
    </div>
</div>

