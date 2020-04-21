# Tracktion Engine Audio Graph Rewrite
## Part 1 - Introduction
###### - Dave Rowland (21/04/20)
___
The audio processing graph inside Tracktion Engine has been around in some form for nearly 20 years now. It's been through a couple of minor changes such as the ability to sum the mix bus in double precision and the addition of multi-threaded track processing but it's largely stayed the same structure. And there's certain things about this structure that make it very difficult to improve any further. So in essence this is the first complete overhaul of the audio graph in 20 years. Let's make sure it's good.

I've decided to document the process here for a few reasons. Firstly, because I'm documenting it as I go, this will give me an opportunity to ensure I've got everything clear in my own head. A fairly long form of rubber ducking.  
Secondly, because we can. Tracktion Engine is in a fairly unique position being an open source framework so I'm free to actually discuss the nitty gritty internals.   
And finally because it's open source, this should actually be useful for people, serving as documentation. I often get asked to explain how bits of Tracktion Engine work and having a complete resource for the audio graph element should save a lot of time and save me from giving poor explanations in the pub after a few drinks.

### The Problems
So why the need for an audio graph rewrite? I'll try and list out some of the most important reasons here to give an idea of what we're trying to fix.

##### PDC
This is number one reason for making a change. It's not that standard plugin delay compensation doesn't work in the existing engine, if you have a plugin on a track, the source material feeding it will be shifted in time so everything comes out in alignment. It's that there are some complex edge cases where this falls down.

The way PDC currently works is that when the Edit time to render passes through a plugin node, that time gets shifted forwards so that it actually reads data (audio file samples and MIDI notes) from the future relative to other tracks. After the plugin has introduced its latency, everything will line up again. However because of this, we lose the context of where the source material is actually from in time when you have complex bus send/return routings.

Because track processing is multi-threaded, the order in which they get processed is indeterminate. The only way to deal with send/return buses with this indeterminism is to double buffer the audio during each send and introduce a block of latency. When the returns need to process, they can all safely get the previous block of audio knowing all sends have been effectively processed.

This works well for simple cases but falls down when these "bus" nodes feed other "bus" nodes. E.g. if you have a send feeding a return on another track feeding a send being received by a third track, the third track can't align its incoming source audio with that from the send because it doesn't know where exactly in the time line it corresponds to.

There are other places this crops up but that's the crux of the main problem.  
Similarly with all this source time forwarding performed, there are bits of the time line that are never accessible e.g. if a plugin has a 0.2s latency and you start play from time 0.0, the first bit of source material you'll hear is from 0.2s. This can be surprising a lead to annoying misses of transients, and note events etc.

##### Fixed size blocks
Leading on from the previous problem, processing largely has to be done in fixed sized blocks. If certain elements in the graph are double buffered, these buffers need to be the same size each processing call.  
We can get around this in branches of the graph which don't have these double-buffered nodes but it's an annoying and fiddly restriction.

##### Multi-threaded utilisation
As I mentioned earlier, the current graph gets processed by multiple threads but this is done on a track-by-track basis. Whilst this does give much better CPU utilisation than single-threaded processing, it doesn't use the threads as best it could. For example, when processing Racks, these are still single threaded so if a Rack takes a long time to process, the other threads may simply be waiting on it to finish. This work could be spread over all the threads better.

##### Usability
While the current graph works very well, it can be difficult to use and understand. There are several paradigms such as which calls you pass on to inputs, how many channels you'll be provided with, what you should do with leftover ones, how you deal with the playhead etc. that are just generally a bit difficult to understand and prone to misuse. This makes extending the graph a bit tricky at times.  
One of the main aims of this rewrite is to clean up all these rough edges and make it much safer to code with and much easier to extend.


### Aims:
So now we know what's wrong with the existing graph we can define some "aims" or "guidelines" for what we want to achieve. Every time a design decision needs to be made during the development we should refer back to these to ensure we're not deviating from our original goals.
  - Separate graph structure from processing and any model
	- *This will make it easier to test and also replace if we need to in the future for any reason*
  - Ensure nodes can be processed multi-threaded which scales independently of graph complexity
  	- *In theory this should give us the best CPU utilisation*
  - Processing can happen in any sized block (up to the maximum prepared for)
  	- *This will enabled us to reduce aliasing due to fast changing automation and handle looping more accurately*
  - Process calls will only ever get the number of channels to fill that they report
	- *This should avoid difficult decisions about what to do with extra channels*
  - Process calls will always provide empty buffers so nodes can simply "add" in to them
  	- *This makes processing simpler. Multiple sources can all just add in to the buffer without clearing it first and if a node doesn't need to process it can simply return. Clearing buffers is generally a quick CPU operation.*
  - Processing in float or double
	- *Although the current engine can sum in doubles, having a complete 64-bit pipeline should give the most headroom possible.*

### Next steps:
Now we have the aims, we can start coding and for that we need a target. As tackling the entire Tracktion Engine model is a bit too much of a first goal, we'll set our sights a little smaller and aim to rebuild the Rack processing.

This is a good goal as it is one of the fundamental problems in the existing engine, not only CPU utilisation but latency isn't correctly balanced across parallel branches. If we can fix both of these problems we'll be in a good state to focus on the rest of the model.

Also, the types of node used in a Rack are relatively small, we have inputs, outputs, plugins and modifiers. Not the dozens of nodes required for the main model (file readers, MIDI, live inputs, mute/solo etc.). This means we can test the fundamental principles firmly and build from there.

Finally, Racks are self-contained. This should mean we can actually ship a version for testing without having to rewrite the whole audio graph. This should provide us with some excellent feedback.