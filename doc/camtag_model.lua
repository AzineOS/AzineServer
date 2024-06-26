-- camtag_model
-- @short: Define a camera for the 3D processing of a rendertarget
-- @inargs: vid:dst
-- @inargs: vid:dst, numbtl:projection
-- @inargs: vid:dst, float:near
-- @inargs: vid:dst, float:near, float:far
-- @inargs: vid:dst, float:near, float:far, float:fov
-- @inargs: vid:dst, float:near, float:far, float:fov, float:aspect
-- @inargs: vid:dst, float:near, float:far, float:fov, float:aspect, bool:front
-- @inargs: vid:dst, float:near, float:far, float:fov, float:aspect, bool:front, bool:back
-- @inargs: vid:dst, float:near, float:far, float:fov, float:aspect, bool:front, bool:back, float:linew
-- @inargs: vid:dst, float:near, float:far, float:fov, float:aspect, bool:front, bool:back, float:linew, vid:tgt
-- @outargs:
-- @longdescr: This function is used to provide a rendertarget with a camera,
-- enabling the 3D processing part of the pipeline. The object that is used as
-- *dst* will be converted into a camera- type, and it is from the perspective
-- of this object that the 3D scene will be processed. The destination rendertarget
-- is either the primary attachment of the object used for *dst*, or, if the long-form
-- argument is used, a specific rendertarget via *tgt*.
-- The camera parameters are tuned using *near*, *far*, and *aspect* which are used
-- to compute the projection matrix that will be activated. The boolean toggles *front* and
-- *back* determines which sides of the model primitives that will be drawn.
-- Setting the *linew* argument to something larger than 0 will enable wireframe
-- drawing mode for the camera and disable normal processing.
-- the argument form providing *projection* will replace the current projection matrix of the
-- camera with that of the table.
-- @group: 3d
-- @cfunction: camtag
-- @related: video_3dorder
-- @flags:

