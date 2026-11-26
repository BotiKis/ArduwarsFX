<?xml version="1.0" encoding="UTF-8"?>
<tileset version="1.10" tiledversion="1.11.0" name="terrain" tilewidth="48" tileheight="48" tilecount="40" columns="5">
 <tileoffset x="0" y="8"/>
 <grid orientation="isometric" width="48" height="24"/>
 <image source="terrain.png" width="256" height="384"/>
 <wangsets>
  <wangset name="Shore" type="corner" tile="15">
   <wangcolor name="Grass" color="#63ab3f" tile="15" probability="1"/>
   <wangcolor name="Water" color="#4fa4b8" tile="0" probability="1"/>
   <wangtile tileid="0" wangid="0,2,0,2,0,2,0,2"/>
   <wangtile tileid="1" wangid="0,2,0,2,0,1,0,2"/>
   <wangtile tileid="2" wangid="0,2,0,1,0,2,0,2"/>
   <wangtile tileid="3" wangid="0,2,0,1,0,1,0,2"/>
   <wangtile tileid="4" wangid="0,1,0,2,0,2,0,2"/>
   <wangtile tileid="5" wangid="0,1,0,2,0,1,0,2"/>
   <wangtile tileid="6" wangid="0,1,0,1,0,2,0,2"/>
   <wangtile tileid="7" wangid="0,1,0,1,0,1,0,2"/>
   <wangtile tileid="8" wangid="0,2,0,2,0,2,0,1"/>
   <wangtile tileid="9" wangid="0,2,0,2,0,1,0,1"/>
   <wangtile tileid="10" wangid="0,2,0,1,0,2,0,1"/>
   <wangtile tileid="11" wangid="0,2,0,1,0,1,0,1"/>
   <wangtile tileid="12" wangid="0,1,0,2,0,2,0,1"/>
   <wangtile tileid="13" wangid="0,1,0,2,0,1,0,1"/>
   <wangtile tileid="14" wangid="0,1,0,1,0,2,0,1"/>
   <wangtile tileid="15" wangid="0,1,0,1,0,1,0,1"/>
  </wangset>
  <wangset name="Roads" type="edge" tile="35">
   <wangcolor name="Road" color="#404973" tile="35" probability="1"/>
   <wangtile tileid="21" wangid="0,0,0,0,0,0,1,0"/>
   <wangtile tileid="22" wangid="0,0,0,0,1,0,0,0"/>
   <wangtile tileid="23" wangid="0,0,0,0,1,0,1,0"/>
   <wangtile tileid="24" wangid="0,0,1,0,0,0,0,0"/>
   <wangtile tileid="25" wangid="0,0,1,0,0,0,1,0"/>
   <wangtile tileid="26" wangid="0,0,1,0,1,0,0,0"/>
   <wangtile tileid="27" wangid="0,0,1,0,1,0,1,0"/>
   <wangtile tileid="28" wangid="1,0,0,0,0,0,0,0"/>
   <wangtile tileid="29" wangid="1,0,0,0,0,0,1,0"/>
   <wangtile tileid="30" wangid="1,0,0,0,1,0,0,0"/>
   <wangtile tileid="31" wangid="1,0,0,0,1,0,1,0"/>
   <wangtile tileid="32" wangid="1,0,1,0,0,0,0,0"/>
   <wangtile tileid="33" wangid="1,0,1,0,0,0,1,0"/>
   <wangtile tileid="34" wangid="1,0,1,0,1,0,0,0"/>
   <wangtile tileid="35" wangid="1,0,1,0,1,0,1,0"/>
  </wangset>
 </wangsets>
</tileset>
