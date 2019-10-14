# Contributing
You are invited to contribute new features, fixes, or updates, large or small; we welcome pull requests, and do our best to process them as fast as we can. However, _please be patient if it takes some time for us to respond to issues and pull requests_.

We strive to maintain a stable version in the master branch, and try new ideas, apply large fixes, or add enhancements in one or more development branches. While you're welcome to submit pull requests against any branch, it is simpler to use an appropriate development branch for new contributions.

Before you start to code, we recommend discussing your plans using a GitHub issue, especially for more ambitious contributions. This gives other contributors a chance to point you in the right direction, give you feedback on your design, and help you find out if someone else is working on the same thing. If you want to raise an issue such as a defect or an enhancement request, please open a GitHub issue for the appropriate topic. Please keep the following in mind:

* If we can't reproduce the issue, we can't fix it. Please detail your environment, and the steps needed to reproduce the issue.
* Try to reduce your code to the bare minimum needed to reproduce the issue.
* If possible, include a suggested fix.
    
# Submitting code pull requests
To make a pull request you will need a GitHub account. For help, see [GitHub's flow documentation](https://guides.github.com/introduction/flow/) on forking and pull requests. Here is the basic flow:

## Setup Git on your build machine
1. Run the following commands on your build system to identify yourself to git

    git config --global user.name “YOUR NAME”  
    git config --global user.email “YOUR EMAIL”  
  
Run ```git config -l``` to verify the configuration.  
2. Add your ssh key to Github as described at <https://help.github.com/en/enterprise/2.18/user/articles/adding-a-new-ssh-key-to-your-github-account>   
3. You can now validate whether the ssh-key is working by running the below command on build system.  
  
    ssh -T git@github.com  

## Fork your copy of OpenFAM Repository on Github  
By default you will not have permission to push changes to the OpenFAM Repository.  GitHub expects users to fork a copy and work on that copy. To Fork a copy follow the steps below.  
1. Go to the [OpenFAM repository](https://github.com/OpenFAM/OpenFAM)  
2. Click on the Fork button at the top right corner.  
This will create a fork of the OpenFAM repository under your [github account](https://github.com/your-user-name/OpenFAM)  

## Clone your forked repository on your build system and start working on it  
The clone can be created and worked on using the following commands:

    $ git clone git@github.com:your-user-name/OpenFAM.git  
    $ cd OpenFAM  
    $ git remote add upstream git@github.com:OpenFAM/OpenFAM.git
      
[More info](https://help.github.com/en/enterprise/2.18/user/articles/fork-a-repo)  

## Sync your working repository with upstream regularly  
Before you push the change to github make sure that your repository is synced with the latest changes. This can be achieved by running the following:
 
    $ git fetch upstream  
    $ git rebase upstream/master  

[More info](https://help.github.com/enterprise/2.18/user/articles/syncing-a-fork/)  
Note: we have used *rebase* instead of *merge* as this will create better log.  

## Commit message format
While committing your changes the commit message heading should have the format  
```Fixed #<issue no.> : <Short description: Not more that 80 chars>```   
For example, if you are submitting changes to issue #10 in github then the commit message should be  
```Fixed #10 : Short description```   
## Push the changes to your forked repository.  
You can push the changes to your repository on github using the command.
  
    $ git push origin master  

Note that push will only push the changes to your forked repository in github.  

## Submit the changes to the mainline  
When your changes in forked repository are ready to be committed to the mainline initiate a pull request.  
This can be triggered by clicking on the green button on the left top corner in [your repository](https://github.com/your-user-name/OpenFAM)  

[More info](https://help.github.com/enterprise/2.18/user/articles/using-pull-requests/)

One of the owners will review your change and merge it into main line if the changes are acceptable.  

## Addressing Review comments  
1. Make the changes to address the review comments.  
2. Do another commit with new commit message.  
3. Push the change to forked repository after rebasing with upstream.  
4. Let the reviewer know about the new push.  
5. The reviewer will now validate the new changes against your previous commit.  
6. Repeat the above steps till all the review comments are addressed.  
7. Once the reviewer gives the final approval  
8. squash your commits into a single commit.  

All pull requests with code should include tests that validate your change. If your pull request is not accepted at first, please try again after addressing the feedback you received. The project requires that each pull request must be reviewed by at least one other reviewer before being accepted.

# Sign your work

All contributions must include acceptance of the [DCO](https://developercertificate.org/) reproduced below:
> Developer Certificate of Origin
> Version 1.1
> 
> Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
>
> 1 Letterman Drive
> Suite D4700
> San Francisco, CA, 94129
> 
> Everyone is permitted to copy and distribute verbatim copies of this
> license document, but changing it is not allowed.
> 
> Developer's Certificate of Origin 1.1
> 
> By making a contribution to this project, I certify that:
> 
> (a) The contribution was created in whole or in part by me and I
>     have the right to submit it under the open source license
>    indicated in the file; or
> 
> (b) The contribution is based upon previous work that, to the best
>     of my knowledge, is covered under an appropriate open source
>     license and I have the right under that license to submit that
>     work with modifications, whether created in whole or in part
>     by me, under the same open source license (unless I am
>     permitted to submit under a different license), as indicated
>     in the file; or
> 
> (c) The contribution was provided directly to me by some other
>     person who certified (a), (b) or (c) and I have not modified
>     it.
> 
> (d) I understand and agree that this project and the contribution
>     are public and that a record of the contribution (including all
>     personal information I submit with it, including my sign-off) is
>     maintained indefinitely and may be redistributed consistent with
>     this project or the open source license(s) involved.
> 

To accept the DCO, simply add the following line to each commit message with your name and email address (git commit -s will do this for you):

>    Signed-off-by: Jane Example <jane@example.com>

For legal reasons, no anonymous or pseudonymous contributions are accepted.

# Other ways to contribute

If you don't feel like writing code, you can still contribute!

1.	You may submit updates and improvements to the documentation.
2.	Submit articles and guides which are also part of the documentation.
3.	Help answer questions and participate in discussion.
4.	Install and test on different platforms
5.	Contribute datasets, test applications, or benchmark results.
